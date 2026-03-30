#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DumbbellManual");

/* ======== CWND ======== */
static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
    *stream->GetStream() << Simulator::Now().GetSeconds()
                         << "\t" << newCwnd << std::endl;
}

/* ======== THROUGHPUT ======== */
void
TraceThroughput(Ptr<PacketSink> sink, Ptr<OutputStreamWrapper> stream)
{
    static uint64_t last = 0;
    uint64_t cur = sink->GetTotalRx();

    double thr = (cur - last) * 8.0 / 1e6; // Mbps

    *stream->GetStream() << Simulator::Now().GetSeconds()
                         << "\t" << thr << std::endl;

    last = cur;
    Simulator::Schedule(Seconds(0.1), &TraceThroughput, sink, stream);
}

int main (int argc, char *argv[])
{
    std::string tcpType = "TcpReno";
    double simTime = 120.0;

    CommandLine cmd;
    cmd.AddValue("tcpType", "TcpReno ou TcpCubic", tcpType);
    cmd.Parse(argc, argv);

    /* ======== TCP ======== */
    if (tcpType == "TcpReno") {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType",
            StringValue("ns3::TcpNewReno"));
    } else {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType",
            StringValue("ns3::TcpCubic"));
    }

    /* ======== NÓS ======== */
    NodeContainer S1, S2, R1, R2, D;
    S1.Create(1); // TCP
    S2.Create(1); // UDP
    R1.Create(1);
    R2.Create(1);
    D.Create(1);  // destino único

    InternetStackHelper stack;
    stack.InstallAll();

    /* ======== LINKS ======== */
    PointToPointHelper leaf, bottle;

    // folhas (acesso)
    leaf.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    leaf.SetChannelAttribute("Delay", StringValue("40ms"));

    // gargalo
    bottle.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    bottle.SetChannelAttribute("Delay", StringValue("20ms"));

    // conexões
    NetDeviceContainer d_S1_R1 = leaf.Install(S1.Get(0), R1.Get(0));
    NetDeviceContainer d_S2_R1 = leaf.Install(S2.Get(0), R1.Get(0));
    NetDeviceContainer d_R1_R2 = bottle.Install(R1.Get(0), R2.Get(0));
    NetDeviceContainer d_R2_D  = leaf.Install(R2.Get(0), D.Get(0));

    /* ======== IP ======== */
    Ipv4AddressHelper addr;

    addr.SetBase("10.1.1.0", "255.255.255.0");
    auto i_S1_R1 = addr.Assign(d_S1_R1);

    addr.SetBase("10.1.2.0", "255.255.255.0");
    auto i_S2_R1 = addr.Assign(d_S2_R1);

    addr.SetBase("10.1.3.0", "255.255.255.0");
    auto i_R1_R2 = addr.Assign(d_R1_R2);

    addr.SetBase("10.1.4.0", "255.255.255.0");
    auto i_R2_D  = addr.Assign(d_R2_D);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t tcpPort = 5000;
    uint16_t udpPort = 4000;

    /* ======== SINK (DESTINO) ======== */
    PacketSinkHelper tcpSink("ns3::TcpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), tcpPort));

    ApplicationContainer tcpSinkApp = tcpSink.Install(D.Get(0));
    tcpSinkApp.Start(Seconds(0.0));

    PacketSinkHelper udpSink("ns3::UdpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), udpPort));

    udpSink.Install(D.Get(0)).Start(Seconds(0.0));

    /* ======== S1 (TCP) ======== */
    BulkSendHelper tcp("ns3::TcpSocketFactory",
        InetSocketAddress(i_R2_D.GetAddress(1), tcpPort));

    tcp.SetAttribute("MaxBytes", UintegerValue(0));

    tcp.Install(S1.Get(0)).Start(Seconds(1.0));

    /* ======== S2 (UDP BACKGROUND) ======== */
    OnOffHelper udp("ns3::UdpSocketFactory",
        InetSocketAddress(i_R2_D.GetAddress(1), udpPort));

    udp.SetConstantRate(DataRate("8Mbps"), 1024);

    udp.Install(S2.Get(0)).Start(Seconds(0.0));

    /* ======== TRACE ======== */
    AsciiTraceHelper ascii;

    auto cwndStream = ascii.CreateFileStream("cwnd-" + tcpType + ".dat");
    auto thrStream  = ascii.CreateFileStream("throughput-" + tcpType + ".dat");

    // CWND (espera socket existir)
    Simulator::Schedule(Seconds(1.1), [&](){
        Config::ConnectWithoutContext(
            "/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
            MakeBoundCallback(&CwndChange, cwndStream));
    });

    // Throughput
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(tcpSinkApp.Get(0));
    Simulator::Schedule(Seconds(1.1), &TraceThroughput, sink, thrStream);

    /* ======== RUN ======== */
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
