#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h" // ⭐ IMPORTANTE
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DumbbellFinal");

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

    double thr = (cur - last) * 8.0 / 1e6;

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
            TypeIdValue(TypeId::LookupByName("ns3::TcpNewReno")));
    } else {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType",
            TypeIdValue(TypeId::LookupByName("ns3::TcpCubic")));
    }

    /* ======== DUMBBELL ======== */
    PointToPointHelper leaf, bottle;

    leaf.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    leaf.SetChannelAttribute("Delay", StringValue("10ms"));

    bottle.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    bottle.SetChannelAttribute("Delay", StringValue("10ms"));
    bottle.SetQueue("ns3::DropTailQueue","MaxSize", StringValue("100p"));

    PointToPointDumbbellHelper d(2, leaf, 2, leaf, bottle);

    InternetStackHelper stack;
    d.InstallStack(stack);

    d.AssignIpv4Addresses(
        Ipv4AddressHelper("10.1.1.0", "255.255.255.0"),
        Ipv4AddressHelper("10.2.1.0", "255.255.255.0"),
        Ipv4AddressHelper("10.3.1.0", "255.255.255.0")
    );

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t tcpPort = 5000;
    uint16_t udpPort = 4000;

    /* ======== SINK TCP (D1) ======== */
    PacketSinkHelper tcpSink("ns3::TcpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), tcpPort));

    ApplicationContainer tcpSinkApp = tcpSink.Install(d.GetRight(0));
    tcpSinkApp.Start(Seconds(0.0));

    /* ======== SINK UDP (D2) ======== */
    PacketSinkHelper udpSink("ns3::UdpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), udpPort));

    udpSink.Install(d.GetRight(1)).Start(Seconds(0.0));

    /* ======== TCP (S1 → D1) ======== */
    BulkSendHelper tcp("ns3::TcpSocketFactory",
        InetSocketAddress(d.GetRightIpv4Address(0), tcpPort));

    tcp.SetAttribute("MaxBytes", UintegerValue(0));

    tcp.Install(d.GetLeft(0)).Start(Seconds(1.0));

    /* ======== UDP (S2 → D2) ======== */
    OnOffHelper udp("ns3::UdpSocketFactory",
        InetSocketAddress(d.GetRightIpv4Address(1), udpPort));

    udp.SetConstantRate(DataRate("8Mbps"), 1024);

    udp.Install(d.GetLeft(1)).Start(Seconds(0.0));

    /* ======== TRACE ======== */
    AsciiTraceHelper ascii;

    auto cwndStream = ascii.CreateFileStream("cwnd-" + tcpType + "fila.dat");
    auto thrStream  = ascii.CreateFileStream("throughput-" + tcpType + "fila.dat");

    Simulator::Schedule(Seconds(2.0), [&](){
        Config::ConnectWithoutContext(
            "/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/CongestionWindow",
            MakeBoundCallback(&CwndChange, cwndStream));
    });

    Ptr<PacketSink> sink = DynamicCast<PacketSink>(tcpSinkApp.Get(0));
    Simulator::Schedule(Seconds(1.1), &TraceThroughput, sink, thrStream);

    /* ======== RUN ======== */
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
