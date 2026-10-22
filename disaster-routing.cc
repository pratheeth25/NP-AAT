// Simple disaster-area ad-hoc routing simulation for ns-3.
// Coordinates are a synthetic 50-node study grid around Chooralmala, Wayanad.
// They are NOT actual locations of survivors, responders, or devices.

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <iomanip>

using namespace ns3;

struct NodePosition {
    uint32_t id;
    double lat;
    double lon;
};

static std::vector<NodePosition>
ReadNodes(const std::string& filename)
{
    std::vector<NodePosition> nodes;
    std::ifstream file(filename);
    if (!file.is_open())
    {
        throw std::runtime_error("Cannot open nodes CSV: " + filename);
    }

    std::string line;
    std::getline(file, line); // header
    while (std::getline(file, line))
    {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string item;
        NodePosition p{};
        std::getline(ss, item, ','); p.id = static_cast<uint32_t>(std::stoul(item));
        std::getline(ss, item, ','); p.lat = std::stod(item);
        std::getline(ss, item, ','); p.lon = std::stod(item);
        nodes.push_back(p);
    }
    if (nodes.size() < 2)
    {
        throw std::runtime_error("Nodes CSV must contain at least two nodes.");
    }
    return nodes;
}

int main(int argc, char* argv[])
{
    std::string nodesFile = "topology_nodes.csv";
    std::string protocol = "AODV";
    std::string csvFile = "metrics.csv";
    uint32_t sources = 8;
    uint32_t seed = 7;
    double simTime = 120.0;
    double radioRange = 180.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nodesFile", "CSV with node_id,latitude,longitude,role", nodesFile);
    cmd.AddValue("protocol", "Routing protocol: AODV, OLSR, or DSDV", protocol);
    cmd.AddValue("csv", "Output metrics CSV path", csvFile);
    cmd.AddValue("sources", "Number of source nodes sending to node 0", sources);
    cmd.AddValue("seed", "Random seed", seed);
    cmd.AddValue("time", "Simulation duration in seconds", simTime);
    cmd.AddValue("radioRange", "Maximum Wi-Fi range in metres", radioRange);
    cmd.Parse(argc, argv);

    if (protocol != "AODV" && protocol != "OLSR" && protocol != "DSDV")
    {
        std::cerr << "ERROR: protocol must be AODV, OLSR, or DSDV\n";
        return 2;
    }

    RngSeedManager::SetSeed(seed);
    RngSeedManager::SetRun(1);

    std::vector<NodePosition> positions = ReadNodes(nodesFile);
    if (sources < 1 || sources >= positions.size())
    {
        std::cerr << "ERROR: sources must be between 1 and node_count - 1\n";
        return 2;
    }

    NodeContainer nodes;
    nodes.Create(positions.size());

    // Convert GPS coordinates to a local flat XY plane, suitable for this small area.
    const double latOrigin = positions[0].lat;
    const double lonOrigin = positions[0].lon;
    Ptr<ListPositionAllocator> allocator = CreateObject<ListPositionAllocator>();
    for (const auto& p : positions)
    {
        double y = (p.lat - latOrigin) * 111320.0;
        double x = (p.lon - lonOrigin) * 111320.0 * std::cos(latOrigin * M_PI / 180.0);
        allocator->Add(Vector(x, y, 0.0));
    }
    MobilityHelper mobility;
    mobility.SetPositionAllocator(allocator);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::RangePropagationLossModel",
                               "MaxRange", DoubleValue(radioRange));
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("DsssRate11Mbps"),
                                 "ControlMode", StringValue("DsssRate1Mbps"));
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    InternetStackHelper internet;
    if (protocol == "AODV")
    {
        AodvHelper routing;
        internet.SetRoutingHelper(routing);
    }
    else if (protocol == "OLSR")
    {
        OlsrHelper routing;
        internet.SetRoutingHelper(routing);
    }
    else
    {
        DsdvHelper routing;
        internet.SetRoutingHelper(routing);
    }
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Command-center node 0 receives UDP packets from nodes 1..sources.
    uint16_t port = 9000;
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sinkHelper.Install(nodes.Get(0));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(simTime));

    for (uint32_t i = 1; i <= sources; ++i)
    {
        OnOffHelper client("ns3::UdpSocketFactory",
            InetSocketAddress(interfaces.GetAddress(0), port));
        client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        client.SetAttribute("DataRate", DataRateValue(DataRate("128kbps")));
        client.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer app = client.Install(nodes.Get(i));
        app.Start(Seconds(10.0 + i * 0.15));
        app.Stop(Seconds(simTime - 1.0));
    }

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    auto stats = monitor->GetFlowStats();

    std::ofstream out(csvFile);
    if (!out.is_open())
    {
        std::cerr << "ERROR: cannot write metrics CSV: " << csvFile << "\n";
        Simulator::Destroy();
        return 3;
    }
    out << "protocol,nodes,sources,sim_time_s,seed,flow_id,src,dst,tx_packets,rx_packets,lost_packets,pdr_percent,throughput_kbps,mean_delay_ms\n";
    out << std::fixed << std::setprecision(3);

    for (const auto& entry : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(entry.first);
        if (t.destinationPort != port || t.sourceAddress == interfaces.GetAddress(0)) continue;
        const auto& s = entry.second;
        double pdr = s.txPackets ? (100.0 * s.rxPackets / s.txPackets) : 0.0;
        double duration = (s.timeLastRxPacket - s.timeFirstTxPacket).GetSeconds();
        double throughput = duration > 0.0 ? (s.rxBytes * 8.0 / duration / 1000.0) : 0.0;
        double delay = s.rxPackets ? (s.delaySum.GetMilliSeconds() / static_cast<double>(s.rxPackets)) : 0.0;
        out << protocol << "," << positions.size() << "," << sources << "," << simTime
            << "," << seed << "," << entry.first << ","
            << t.sourceAddress << "," << t.destinationAddress << ","
            << s.txPackets << "," << s.rxPackets << "," << s.lostPackets << ","
            << pdr << "," << throughput << "," << delay << "\n";
    }
    out.close();

    std::cout << "Protocol: " << protocol << "\n"
              << "Nodes: " << positions.size() << ", sources: " << sources << "\n"
              << "Metrics written to: " << csvFile << "\n";
    Simulator::Destroy();
    return 0;
}
