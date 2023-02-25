/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
//third.cc sixth.cc
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include <fstream>
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"

// Default Network Topology
//
//   Wifi 10.1.3.0
//                 AP
//  *    *    *    *
//  |    |    |    |    10.1.1.0
// n5   n6   n7   n0 -------------- n1   n2   n3   n4
//                   point-to-point  |    |    |    |
//                                   *    *    *    *
//                                     Wifi 10.1.2.0

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("taskA");

void CalculateThroughput(Ptr<PacketSink> sink, Ptr<OutputStreamWrapper> stream, uint64_t lastTotalRx)
{
    Time now = Simulator::Now();                                       /* Return the simulator's virtual time. */
    double cur = (sink->GetTotalRx() - lastTotalRx) * (double)8 / 1e5; /* Convert Application RX Packets to MBits.  /100 ms * 1e6 */
    if (cur != 0)
        *stream->GetStream() << now.GetSeconds() << "\t" << cur << std::endl;
    //std::cout << now.GetSeconds() << "s: \t" << cur << " Mbit/s" << std::endl;
    lastTotalRx = sink->GetTotalRx();
    Simulator::Schedule(MilliSeconds(100), &CalculateThroughput, sink, stream, lastTotalRx);
}

class MyApp : public Application
{
public:
    MyApp();
    virtual ~MyApp();

    /**
   * Register this type.
   * \return The TypeId.
   */
    static TypeId GetTypeId(void);
    void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void ScheduleTx(void);
    void SendPacket(void);

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    DataRate m_dataRate;
    EventId m_sendEvent;
    bool m_running;
    uint32_t m_packetsSent;
};

MyApp::MyApp()
    : m_socket(0),
      m_peer(),
      m_packetSize(0),
      m_nPackets(0),
      m_dataRate(0),
      m_sendEvent(),
      m_running(false),
      m_packetsSent(0)
{
}

MyApp::~MyApp()
{
    m_socket = 0;
}

/* static */
TypeId MyApp::GetTypeId(void)
{
    static TypeId tid = TypeId("MyApp")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial")
                            .AddConstructor<MyApp>();
    return tid;
}

void MyApp::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_dataRate = dataRate;
}

void MyApp::StartApplication(void)
{
    m_running = true;
    m_packetsSent = 0;
    m_socket->Bind();
    m_socket->Connect(m_peer);
    SendPacket();
}

void MyApp::StopApplication(void)
{
    m_running = false;

    if (m_sendEvent.IsRunning())
    {
        Simulator::Cancel(m_sendEvent);
    }

    if (m_socket)
    {
        m_socket->Close();
    }
}

void MyApp::SendPacket(void)
{
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);

    if (++m_packetsSent < m_nPackets)
    {
        ScheduleTx();
    }
}

void MyApp::ScheduleTx(void)
{
    if (m_running)
    {
        Time tNext(Seconds(m_packetSize * 8 / static_cast<double>(m_dataRate.GetBitRate())));
        m_sendEvent = Simulator::Schedule(tNext, &MyApp::SendPacket, this);
    }
}
/*
static void
CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
    //std::cout << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << oldCwnd << "\t" << newCwnd << std::endl;
}
*/
int main(int argc, char *argv[])
{
    bool verbose = true;
    uint32_t leftnodes = 26;
    uint32_t rightnodes = 26;
    bool tracing = false;
    uint32_t num_half_flows = 5;
    int nPackets = 500;
    std::string bottleNeckDataRate = "5Mbps";

    uint32_t payloadSize = 1472;           /* Transport layer payload size in bytes. */
    std::string dataRate = "50Mbps";       /* Application layer datarate. */
    std::string tcpVariant = "TcpNewReno"; /* TCP variant type. */
    std::string phyRate = "HtMcs7";        /* Physical layer bitrate. */
    double simulationTime = 8;            /* Simulation time in seconds. */
    bool pcapTracing = true;               /* PCAP Tracing is enabled or not. */
    uint32_t velocity = 5;
    uint32_t p = 100;
    std::string filename;

    /* Command line argument parser setup. */
    CommandLine cmd(__FILE__);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("dataRate", "Application data ate", dataRate);
    cmd.AddValue("tcpVariant", "Transport protocol to use: TcpNewReno, "
                               "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                               "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat ",
                 tcpVariant);
    cmd.AddValue("phyRate", "Physical layer bitrate", phyRate);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("pcap", "Enable/disable PCAP Tracing", pcapTracing);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.AddValue("n", "Number of \"extra\" CSMA nodes/devices", leftnodes);
    cmd.AddValue("right", "Number of wifi STA devices", rightnodes);
    cmd.AddValue("flow", "how may flow", num_half_flows);
    cmd.AddValue("v", "velocity of nodes", velocity);
    cmd.AddValue("npacket", "number of packets", nPackets);
    cmd.AddValue("p", "packets per second ", p);
    cmd.AddValue("file", "enter file name", filename);
    cmd.Parse(argc, argv);

    tcpVariant = std::string("ns3::") + tcpVariant;
    std::cout << "previous data rate :" << dataRate << std::endl;
    std::ostringstream datarate;
    uint32_t rate = payloadSize * p * 8 / 1e6;
    datarate << rate << "Mbps";
    dataRate = datarate.str();
    std::cout << "current data rate :" << dataRate << std::endl;

    
    rightnodes=leftnodes;

    // Select TCP variant
    if (tcpVariant.compare("ns3::TcpWestwoodPlus") == 0)
    {
        // TcpWestwoodPlus is not an actual TypeId name; we need TcpWestwood here
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId()));
        // the default protocol type in ns3::TcpWestwood is WESTWOOD
        Config::SetDefault("ns3::TcpWestwood::ProtocolType", EnumValue(TcpWestwood::WESTWOODPLUS));
    }
    else
    {
        TypeId tcpTid;
        NS_ABORT_MSG_UNLESS(TypeId::LookupByNameFailSafe(tcpVariant, &tcpTid), "TypeId " << tcpVariant << " not found");
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName(tcpVariant)));
    }

    /* Configure TCP Options */
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(payloadSize));

   
    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer p2pNodes;
    p2pNodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(bottleNeckDataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices;
    p2pDevices = pointToPoint.Install(p2pNodes);

    //left wifi network
    NodeContainer wifiStaNodesLeft;
    wifiStaNodesLeft.Create(leftnodes);
    NodeContainer wifiApNodeLeft = p2pNodes.Get(0);

    /* Configure TCP Options */
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(payloadSize));
    WifiMacHelper macLeft;
    WifiHelper wifiLeft;

    wifiLeft.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    /* Set up Legacy Channel */
    YansWifiChannelHelper channelLeft;
    channelLeft.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channelLeft.AddPropagationLoss("ns3::FriisPropagationLossModel", "Frequency", DoubleValue(5e9));
    /* Setup Physical Layer */
    YansWifiPhyHelper phyLeft;

    phyLeft.SetChannel(channelLeft.Create());
    phyLeft.SetErrorRateModel("ns3::YansErrorRateModel");
    wifiLeft.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue(phyRate),
                                     "ControlMode", StringValue("HtMcs0"));
    Ssid ssid = Ssid("ns-3-ssid");
    macLeft.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevicesLeft;
    staDevicesLeft = wifiLeft.Install(phyLeft, macLeft, wifiStaNodesLeft);
    macLeft.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevicesLeft;
    apDevicesLeft = wifiLeft.Install(phyLeft, macLeft, wifiApNodeLeft);

    //right wifi network
    NodeContainer wifiStaNodesRight;
    wifiStaNodesRight.Create(rightnodes);
    NodeContainer wifiApNodeRight = p2pNodes.Get(1);

    WifiMacHelper macRight;
    WifiHelper wifiRight;
    wifiRight.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    /* Set up Legacy Channel */
    YansWifiChannelHelper channelRight;
    channelRight.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channelRight.AddPropagationLoss("ns3::FriisPropagationLossModel", "Frequency", DoubleValue(5e9));
    /* Setup Physical Layer */
    YansWifiPhyHelper phyRight;

    phyRight.SetChannel(channelRight.Create());
    phyRight.SetErrorRateModel("ns3::YansErrorRateModel");
    wifiRight.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue(phyRate),
                                      "ControlMode", StringValue("HtMcs0"));
    Ssid ssid2 = Ssid("ns-3-ssid");
    macRight.SetType("ns3::StaWifiMac",
                     "Ssid", SsidValue(ssid2),
                     "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevicesRight;
    staDevicesRight = wifiRight.Install(phyRight, macRight, wifiStaNodesRight);
    macRight.SetType("ns3::ApWifiMac",
                     "Ssid", SsidValue(ssid2));
    NetDeviceContainer apDevicesRight;
    apDevicesRight = wifiRight.Install(phyRight, macRight, wifiApNodeRight);
    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(30),
                                  "LayoutType", StringValue("RowFirst"));

    //mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
    //                          "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(wifiStaNodesLeft);
    mobility.Install(wifiStaNodesRight);
    Ptr<ConstantVelocityMobilityModel> mob;
    for (uint32_t i = 0; i < leftnodes; i++)
    {
        mob = wifiStaNodesLeft.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        mob->SetVelocity(Vector(velocity, 0.0, 0.0));
    }
    for (uint32_t i = 0; i < rightnodes; i++)
    {
        mob = wifiStaNodesRight.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        mob->SetVelocity(Vector(velocity, 0.0, 0.0));
    }
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNodeLeft);
    mobility.Install(wifiApNodeRight);

    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetAttribute("ErrorRate", DoubleValue(0.00001));
    p2pDevices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    p2pDevices.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));

    InternetStackHelper stack;
    stack.Install(wifiApNodeLeft);
    stack.Install(wifiStaNodesLeft);
    stack.Install(wifiApNodeRight);
    stack.Install(wifiStaNodesRight);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces;
    p2pInterfaces = address.Assign(p2pDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer rightInterfaces;
    rightInterfaces = address.Assign(staDevicesRight);
    address.Assign(apDevicesRight);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer leftInterfaces;
    leftInterfaces = address.Assign(staDevicesLeft);
    address.Assign(apDevicesLeft);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t sinkPort = 8080;
    for (uint32_t i = 1; i <= num_half_flows; i++)
    {

        Address sinkAddress(InetSocketAddress(leftInterfaces.GetAddress(leftnodes - 1 - i), sinkPort));

        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
        ApplicationContainer sinkApps = packetSinkHelper.Install(wifiStaNodesLeft.Get(leftnodes - 1 - i));
        Ptr<PacketSink> sink = StaticCast<PacketSink>(sinkApps.Get(0));
        sinkApps.Start(Seconds(0.));
        sinkApps.Stop(Seconds(simulationTime));

        Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(wifiStaNodesRight.Get(i), TcpSocketFactory::GetTypeId());
        Ptr<MyApp> app = CreateObject<MyApp>();
        app->Setup(ns3TcpSocket, sinkAddress, payloadSize, nPackets, DataRate(dataRate));
        wifiStaNodesRight.Get(i)->AddApplication(app);

        app->SetStartTime(Seconds(1.));
        app->SetStopTime(Seconds(simulationTime));
        NS_LOG_UNCOND("source: " << rightInterfaces.GetAddress(i) << " destination: " << leftInterfaces.GetAddress(leftnodes - 1 - i));
        AsciiTraceHelper asciiTraceHelper;
        std::ostringstream oss;
        oss << "./simulation_output/flow" << i << ".cwnd";

    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));

    AsciiTraceHelper ascii;
    static Ptr<OutputStreamWrapper> flowStream = ascii.CreateFileStream(filename);
    /*phyLeft.EnableAsciiAll(ascii.CreateFileStream("phyL.tr"));
    phyRight.EnableAsciiAll(ascii.CreateFileStream("phyR.tr"));
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("p2p.tr"));*/

    /*
    if (tracing)
    {
        //phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
        pointToPoint.EnablePcapAll("p2pCap");
        phyLeft.EnablePcapAll("phyCapL");
        phyRight.EnablePcapAll("phyCapR");
    }
    */
    Simulator::Run();
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    int total_flow = 0;
    int totalSentPackets = 0;
    int totalReceivedPackets = 0;
    double totalDelay=0;
    double throughput = 0;

    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)

    {

        if (i->first > 0)
        {
            //Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
            /* std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Packets           :" << i->second.txPackets << "\n";
            std::cout << "  Tx Bytes             :" << i->second.txBytes << "\n";
            std::cout << "  TxOffered            :" << i->second.txBytes * 8.0 / (simulationTime - 1) / 1000.0 / 1000 << " Mbps\n";
            std::cout << "  Rx Packets           :" << i->second.rxPackets << "\n";
            std::cout << "  Rx Bytes             :" << i->second.rxBytes << "\n";
            std::cout << "  Lost Packets         :" << i->second.txPackets - i->second.rxPackets << "\n";
            std::cout << "  Packet Loss ratio    :" << (i->second.txPackets - i->second.rxPackets) * 100.0 / i->second.txPackets << "\n";
            std::cout << "  Packet Delivery ratio:" << i->second.rxPackets * 100.0 / i->second.txPackets << "\n";
            std::cout << "  Delay                :" << i->second.delaySum << "\n";
            std::cout << "  Throughput           :" << i->second.rxBytes * 8.0 / (simulationTime - 1) / 1000 / 1000.0 << " Mbps\n";*/
            totalSentPackets += i->second.txPackets;
            totalReceivedPackets += i->second.rxPackets;
            throughput += (i->second.rxBytes * 8.0 / (simulationTime - 1) / 1000 / 1000.0);
            totalDelay += i->second.delaySum.GetSeconds();
            total_flow++;
        }
    }
    throughput =throughput /(2*num_half_flows);
    totalDelay = totalDelay / (2* num_half_flows);
   // *flowStream->GetStream() <<"nodes: "<<leftnodes<<" flow :"<<2*num_half_flows<<" "<<" pkt:"<<p<<" velocity :"<<velocity<<std::endl;
    *flowStream->GetStream() << "drop " << ((totalSentPackets - totalReceivedPackets) * 100.00) / totalSentPackets<<  std::endl;
    *flowStream->GetStream() << "delivery " << ((totalReceivedPackets * 100.00) / totalSentPackets) <<  std::endl;
    *flowStream->GetStream() << "throughput " << throughput << std::endl;
    *flowStream->GetStream() << "delay " << totalDelay << std::endl;
    //*flowStream->GetStream() << "Total Flow " << total_flow << std::endl;
    Simulator::Destroy();

    return 0;
}
