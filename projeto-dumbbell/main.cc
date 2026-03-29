#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/tcp-congestion-ops.h"

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

    double thr = (cur - last) * 8.0 / 1e6; // Mbps

    *stream->GetStream() << Simulator::Now().GetSeconds()
                         << "\t" << thr << std::endl;

    last = cur;
    Simulator::Schedule(Seconds(0.1), &TraceThroughput, sink, stream);
}

int main (int argc, char *argv[])
{
    std::string tcpType = "TcpReno";
    double simTime = 30.0;

    CommandLine cmd;
    cmd.AddValue("tcpType", "TcpReno ou TcpCubic", tcpType);
    cmd.Parse(argc, argv);

    /* ======== TCP ======== */
    if (tcpType == "TcpReno") {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));
    }
    else if (tcpType == "TcpCubic") {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpCubic"));
    }
    else {
        NS_FATAL_ERROR("TCP inválido");
    }

    /* ======== TOPOLOGIA ======== */
    PointToPointHelper leaf, bottle;

    leaf.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    leaf.SetChannelAttribute("Delay", StringValue("1ms"));

    bottle.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    bottle.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointDumbbellHelper d(1, leaf, 1, leaf, bottle);

    InternetStackHelper stack;
    d.InstallStack(stack);

    d.AssignIpv4Addresses(
        Ipv4AddressHelper("10.1.1.0", "255.255.255.0"),
        Ipv4AddressHelper("10.2.1.0", "255.255.255.0"),
        Ipv4AddressHelper("10.3.1.0", "255.255.255.0")
    );

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t udpPort = 4000;
    uint16_t tcpPort = 5000;

    /* ======== UDP BACKGROUND ======== */
    PacketSinkHelper udpSink("ns3::UdpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), udpPort));

    ApplicationContainer udpSinkApp = udpSink.Install(d.GetRight(0));
    udpSinkApp.Start(Seconds(0.0));
    udpSinkApp.Stop(Seconds(simTime));

    OnOffHelper udp("ns3::UdpSocketFactory",
        InetSocketAddress(d.GetRightIpv4Address(0), udpPort));

    udp.SetConstantRate(DataRate("6Mbps"), 1024);

    ApplicationContainer udpApp = udp.Install(d.GetLeft(0));
    udpApp.Start(Seconds(0.0));
    udpApp.Stop(Seconds(simTime));


    /* ======== TCP (REAL) ======== */
    PacketSinkHelper tcpSink("ns3::TcpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), tcpPort));

    ApplicationContainer tcpSinkApp = tcpSink.Install(d.GetRight(0));
    tcpSinkApp.Start(Seconds(0.0));
    tcpSinkApp.Stop(Seconds(simTime));

    BulkSendHelper tcp("ns3::TcpSocketFactory",
        InetSocketAddress(d.GetRightIpv4Address(0), tcpPort));

    tcp.SetAttribute("MaxBytes", UintegerValue(0));

    ApplicationContainer tcpApp = tcp.Install(d.GetLeft(0));
    tcpApp.Start(Seconds(1.0));
    tcpApp.Stop(Seconds(simTime));

    /* ======== TRACE ======== */
    AsciiTraceHelper ascii;

    auto cwndStream = ascii.CreateFileStream("cwnd-" + tcpType + "6.dat");

    Simulator::Schedule(Seconds(1.1), [&](){
        Config::ConnectWithoutContext(
            "/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/CongestionWindow",
            MakeBoundCallback(&CwndChange, cwndStream)
        );
    });

    auto thrStream  = ascii.CreateFileStream("throughput-" + tcpType + "6.dat");

    Ptr<PacketSink> sink = DynamicCast<PacketSink>(tcpSinkApp.Get(0));

    Simulator::Schedule(Seconds(1.1), &TraceThroughput, sink, thrStream);

    /* ======== SIMULAÇÃO ======== */
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
