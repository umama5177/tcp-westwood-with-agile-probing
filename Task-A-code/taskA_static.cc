#include <fstream>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv6-flow-classifier.h"
#include "ns3/ipv6-routing-protocol.h"
#include "ns3/csma-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Lr-WpanStatic");

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

int main(int argc, char **argv)
{
    bool verbose = true;
    //uint32_t nLeft = 20;
    //uint32_t nRight = 20;
    uint32_t nodes = 25;
    uint32_t n = 25;
    uint32_t num_half_flows = 5;
    int nPackets = 1000;
    uint32_t packetSize = 100;
    std::string dataRate = "50Mbps";
    std::string tcpVariant = "TcpNewReno";
    bool sack = true;
    std::string recovery = "ns3::TcpClassicRecovery";
    std::string phyRate = "HtMcs7";
    double simulationTime = 10;
    uint32_t packets_per_second = 100;
    double range = 1;
    uint32_t Tx = 10;
    double minx = -2;
    double miny = -2;
    std::string filename;

    tcpVariant = std::string("ns3::") + tcpVariant;

    TypeId tcpTid;
    NS_ABORT_MSG_UNLESS(TypeId::LookupByNameFailSafe(tcpVariant, &tcpTid), "TypeId " << tcpVariant << " not found");
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName(tcpVariant)));
    Config::SetDefault("ns3::TcpL4Protocol::RecoveryType",
                       TypeIdValue(TypeId::LookupByName(recovery)));

    // 2 MB of TCP buffer
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 21));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1 << 21));
    Config::SetDefault("ns3::TcpSocketBase::Sack", BooleanValue(sack));

    Packet::EnablePrinting();

    CommandLine cmd(__FILE__);
    cmd.AddValue("flow", "no of half flow", num_half_flows);
    cmd.AddValue("n", "wifi nodes", n);
    cmd.AddValue("r", "range", range);
    cmd.AddValue("file", "filename for flowmonitor result", filename);
    cmd.AddValue("p", "packets per sec", packets_per_second);
    cmd.Parse(argc, argv);

    AsciiTraceHelper ascii;
    static Ptr<OutputStreamWrapper> flowStream = ascii.CreateFileStream(filename);
    if (verbose)
    {
        //LogComponentEnable("PacketSink", LOG_LEVEL_ALL);
        LogComponentEnable("Lr-WpanStatic", LOG_LEVEL_ALL);
    }

    nodes = n + 1;

    std::ostringstream datarate;
    uint32_t rate = packetSize * packets_per_second * 8 / 1e3;
    datarate << rate << "Kbps";
    dataRate = datarate.str();
    std::cout << "current data rate :" << dataRate << std::endl;
    /* Configure TCP Options */
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(packetSize));

    Config::SetDefault("ns3::RangePropagationLossModel::MaxRange", DoubleValue(10));
    /*Ptr<SingleModelSpectrumChannel> channel = CreateObject<SingleModelSpectrumChannel>();
    Ptr<RangePropagationLossModel> propModel = CreateObject<RangePropagationLossModel>();
    Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel>();
    channel->AddPropagationLossModel(propModel);
    channel->SetPropagationDelayModel(delayModel);*/

    NodeContainer wsnNodes;
    wsnNodes.Create(nodes);
    NodeContainer wiredNodes;
    wiredNodes.Create(1);
    wiredNodes.Add(wsnNodes.Get(0));
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(range * minx),
                                  "MinY", DoubleValue(range * miny),
                                  "DeltaX", DoubleValue(80),
                                  "DeltaY", DoubleValue(80),
                                  "GridWidth", UintegerValue(Tx*range),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.Install(wsnNodes);

    // creating a channel with range propagation loss model
    /* static node coverage area */

    LrWpanHelper lrWpanHelper;
    // setting the channel in helper
    //lrWpanHelper.SetChannel(channel);

    NetDeviceContainer lrwpanDevices = lrWpanHelper.Install(wsnNodes);
    lrWpanHelper.AssociateToPan(lrwpanDevices, 0);
    InternetStackHelper internetv6;
    internetv6.Install(wsnNodes);
    internetv6.Install(wiredNodes.Get(0));
    SixLowPanHelper sixLowPanHelper;
    NetDeviceContainer sixLowPanDevices = sixLowPanHelper.Install(lrwpanDevices);
    CsmaHelper csmaHelper;
    NetDeviceContainer csmaDevices = csmaHelper.Install(wiredNodes);
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:cafe::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer wiredDeviceInterfaces;
    wiredDeviceInterfaces = ipv6.Assign(csmaDevices);
    wiredDeviceInterfaces.SetForwarding(1, true);
    wiredDeviceInterfaces.SetDefaultRouteInAllNodes(1);
    ipv6.SetBase(Ipv6Address("2001:f00d::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer wsnDeviceInterfaces;
    wsnDeviceInterfaces = ipv6.Assign(sixLowPanDevices);
    wsnDeviceInterfaces.SetForwarding(0, true);
    wsnDeviceInterfaces.SetDefaultRouteInAllNodes(0);
    for (uint32_t i = 0; i < sixLowPanDevices.GetN(); i++)
    {
        Ptr<NetDevice> dev = sixLowPanDevices.Get(i);
        dev->SetAttribute("UseMeshUnder", BooleanValue(true));
        dev->SetAttribute("MeshUnderRadius", UintegerValue(10));
    }

    //left sink right sender
    int sinkPort = 9;
    for (uint32_t i = 0; i < num_half_flows; i++)
    {
        Address sinkAddress = Inet6SocketAddress(Inet6SocketAddress(wsnDeviceInterfaces.GetAddress(i, 1), sinkPort));
        Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(wiredNodes.Get(0), TcpSocketFactory::GetTypeId());
        //Config::Set("/NodeList/[i]/DeviceList/1/$ns3::LrWpanNetDevice/Channel/PropagationLossModel/$ns3::RangePropagationLossModel" );
        Ptr<MyApp> app = CreateObject<MyApp>();
        app->Setup(ns3TcpSocket, sinkAddress, packetSize, nPackets, DataRate(dataRate));
        wiredNodes.Get(0)->AddApplication(app);
        app->SetStartTime(Seconds(1.0));
        app->SetStopTime(Seconds(simulationTime));
        PacketSinkHelper sinkApp("ns3::TcpSocketFactory",
                                 Inet6SocketAddress(Ipv6Address::GetAny(), sinkPort));
        sinkApp.SetAttribute("Protocol", TypeIdValue(TcpSocketFactory::GetTypeId()));
        ApplicationContainer sinkApps = sinkApp.Install(wsnNodes.Get(i));

        Ptr<PacketSink> sink = StaticCast<PacketSink>(sinkApps.Get(0));

        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(Seconds(simulationTime));
        sinkPort++;
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    ///////////////////////// Calculate throughput and FlowMonitor statistics ////////////////////////////////////
    double AvgThroughput = 0;
    double Delay = 0;
    int j = 0;
    uint32_t SentPackets = 0;
    uint32_t ReceivedPackets = 0;
    uint32_t LostPackets = 0;
    Ptr<Ipv6FlowClassifier> classifier = DynamicCast<Ipv6FlowClassifier>(flowmon.GetClassifier6());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
        /*Ipv6FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        // classifier returns FiveTuple in correspondance to a flowID
        *flowStream->GetStream() << "----Flow ID:" << iter->first << std::endl;
        *flowStream->GetStream() << "Src Addr" << t.sourceAddress << " -- Dst Addr " << t.destinationAddress << std::endl;
        *flowStream->GetStream() << "Sent Packets :" << iter->second.txPackets << std::endl;
        *flowStream->GetStream() << "Received Packets :" << iter->second.rxPackets << std::endl;
        *flowStream->GetStream() << "Lost Packets :" << iter->second.txPackets - iter->second.rxPackets << std::endl;
        *flowStream->GetStream() << "Packet delivery ratio :" << iter->second.rxPackets * 100.0 / iter->second.txPackets << "%" << std::endl;
        *flowStream->GetStream() << "Packet loss ratio :" << (iter->second.txPackets - iter->second.rxPackets) * 100.0 / iter->second.txPackets << "%" << std::endl;
        *flowStream->GetStream() << "Delay :" << iter->second.delaySum << std::endl;

        *flowStream->GetStream() << "  Throughput :" << iter->second.rxBytes * 8.0 / (simulationTime - 1) / 1000 / 1000.0 << " Mbps\n";*/

        SentPackets = SentPackets + (iter->second.txPackets);
        ReceivedPackets = ReceivedPackets + (iter->second.rxPackets);
        LostPackets = LostPackets + (iter->second.txPackets - iter->second.rxPackets);

        if (iter->second.rxPackets != 0)
        {
            AvgThroughput = AvgThroughput + (iter->second.rxBytes * 8.0 / (simulationTime - 1) / 1000 / 1000.0);
        }

        //Delay = Delay + (iter->second.delaySum.GetNanoSeconds());
        Delay += iter->second.delaySum.GetSeconds();
        j++;
    }
    //*flowStream->GetStream() <<"nodes: "<<nodes<<" flow :"<<2*num_half_flows<<" "<<" pkt:"<<packets_per_second<<" range :"<<range*Tx<<std::endl;

    AvgThroughput = AvgThroughput / (2 * num_half_flows);
    Delay = Delay / (2 * num_half_flows);

    //*flowStream->GetStream() << "--------Total Results of the simulation----------" << std::endl;
    //*flowStream->GetStream() << "Total sent packets  =" << SentPackets << std::endl;
    //*flowStream->GetStream() << "Total Received Packets =" << ReceivedPackets << std::endl;
    //*flowStream->GetStream() << "Total Lost Packets =" << LostPackets << std::endl;
    *flowStream->GetStream() << "drop " << ((LostPackets * 100.00) / SentPackets) << std::endl;
    *flowStream->GetStream() << "delivery " << ((ReceivedPackets * 100.00) / SentPackets) << std::endl;
    *flowStream->GetStream() << "throughput " << AvgThroughput << std::endl;
    *flowStream->GetStream() << "delay " << Delay << std::endl;
    //*flowStream->GetStream() << "Total Flow  " << j << std::endl;
    //-------------------------------- end flow monitor output----------------------------------------
    Simulator::Destroy();
    return 0;
}