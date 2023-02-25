/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013 ResiliNets, ITTC, University of Kansas 
 *
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
 *
 * Authors: Siddharth Gangadhar <siddharth@ittc.ku.edu>,
 *          Truc Anh N. Nguyen <annguyen@ittc.ku.edu>,
 *          Greeshma Umapathi
 *
 * James P.G. Sterbenz <jpgs@ittc.ku.edu>, director
 * ResiliNets Research Group  http://wiki.ittc.ku.edu/resilinets
 * Information and Telecommunication Technology Center (ITTC)
 * and Department of Electrical Engineering and Computer Science
 * The University of Kansas Lawrence, KS USA.
 *
 * Work supported in part by NSF FIND (Future Internet Design) Program
 * under grant CNS-0626918 (Postmodern Internet Architecture),
 * NSF grant CNS-1050226 (Multilayer Network Resilience Analysis and Experimentation on GENI),
 * US Department of Defense (DoD), and ITTC at The University of Kansas.
 */

#include "tcp-westwoodwithagileprobing.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "rtt-estimator.h"
#include "tcp-socket-base.h"

NS_LOG_COMPONENT_DEFINE("TcpWestwoodWithAgileProbing");

uint32_t segments, c = 0;

namespace ns3
{

  NS_OBJECT_ENSURE_REGISTERED(TcpWestwoodWithAgileProbing);

  TypeId
  TcpWestwoodWithAgileProbing::GetTypeId(void)
  {
    static TypeId tid = TypeId("ns3::TcpWestwoodWithAgileProbing")
                            .SetParent<TcpNewReno>()
                            .SetGroupName("Internet")
                            .AddConstructor<TcpWestwoodWithAgileProbing>()
                            .AddTraceSource("EstimatedBW", "The estimated bandwidth",
                                            MakeTraceSourceAccessor(&TcpWestwoodWithAgileProbing::m_currentBW),
                                            "ns3::TracedValueCallback::Double")

                            .AddTraceSource("CalculateERE", "eligible rate estimate",
                                            MakeTraceSourceAccessor(&TcpWestwoodWithAgileProbing::ERE),
                                            "ns3::TracedValueCallback::Double")

        ;
    return tid;
  }

  TcpWestwoodWithAgileProbing::TcpWestwoodWithAgileProbing(void)
      : TcpNewReno(),
        m_currentBW(0),
        m_lastSampleBW(0),
        m_lastBW(0),
        m_ackedSegments(0),
        m_IsCount(false),
        m_lastAck(0),
        m_minRtt(Time::Max()),
        segment_size(0),
        ackedSegments(0),
        no_congestion_counter(0),
        beta(0.6)

  {
    NS_LOG_FUNCTION(this);
    //Simulator::Schedule(Seconds(0.1), &TcpWestwoodWithAgileProbing::CalculateERE, this);
  }

  TcpWestwoodWithAgileProbing::TcpWestwoodWithAgileProbing(const TcpWestwoodWithAgileProbing &sock)
      : TcpNewReno(sock),
        m_currentBW(sock.m_currentBW),
        m_lastSampleBW(sock.m_lastSampleBW),
        m_lastBW(sock.m_lastBW),
        m_IsCount(sock.m_IsCount),
        m_minRtt(Time::Max()),
        segment_size(0),
        ackedSegments(0),
        no_congestion_counter(0),
        beta(0.6)
  {
    NS_LOG_FUNCTION(this);
    NS_LOG_LOGIC("Invoked the copy constructor");
    //Simulator::Schedule(Seconds(0.1), &TcpWestwoodWithAgileProbing::CalculateERE, this);
  }

  TcpWestwoodWithAgileProbing::~TcpWestwoodWithAgileProbing(void)
  {
  }

  void
  TcpWestwoodWithAgileProbing::CalculateERE()
  {
    uint32_t ackedSegs;
    if (ackedSegments - segments > 0)
      ackedSegs = ackedSegments - segments;
    ERE = ackedSegs * segment_size / Tk;
    Time now = Simulator::Now();
    segments = ackedSegments;
    if (Tk < 0)
      Tk = -Tk;
    if (Tk > 0)
      Simulator::Schedule(Seconds(Tk),
                          &TcpWestwoodWithAgileProbing::CalculateERE, this);
  }

  void
  TcpWestwoodWithAgileProbing::IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
  {
    NS_LOG_FUNCTION(this << tcb << segmentsAcked);

    if (tcb->m_cWnd < tcb->m_ssThresh)
    {
      segmentsAcked = SlowStart(tcb, segmentsAcked);
    }

    if (tcb->m_cWnd >= tcb->m_ssThresh)
    {
      CongestionAvoidance(tcb, segmentsAcked);
    }

    /* At this point, we could have segmentsAcked != 0. This because RFC says
   * that in slow start, we should increase cWnd by min (N, SMSS); if in
   * slow start we receive a cumulative ACK, it counts only for 1 SMSS of
   * increase, wasting the others.
   *
   * // Incorrect assert, I am sorry
   * NS_ASSERT (segmentsAcked == 0);
   */
  }
  uint32_t
  TcpWestwoodWithAgileProbing::SlowStart(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
  {
    NS_LOG_FUNCTION(this << tcb << segmentsAcked);

    if (segmentsAcked >= 1)
    {
      Agileprobing(tcb);
      return segmentsAcked - 1;
    }
    else
    {
    }
    return 0;
  }

  /**
 * \brief NewReno congestion avoidance
 *
 * During congestion avoidance, cwnd is incremented by roughly 1 full-sized
 * segment per round-trip time (RTT).
 *
 * \param tcb internal congestion state
 * \param segmentsAcked count of segments acked
 */
  void
  TcpWestwoodWithAgileProbing::CongestionAvoidance(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
  {
    NS_LOG_FUNCTION(this << tcb << segmentsAcked);

    if (segmentsAcked > 0)
    {
      PersistentNonCongestionDetection(tcb);
      if (restartAP == 1)
      {
        double adder = static_cast<double>(tcb->m_segmentSize * tcb->m_segmentSize) / tcb->m_cWnd.Get();
        adder = std::max(1.0, adder);
        tcb->m_cWnd += tcb->m_segmentSize;
        NS_LOG_INFO("In CongAvoid, updated to cwnd " << tcb->m_cWnd << " ssthresh " << tcb->m_ssThresh);
      }
    }
    else
    {
    }
  }
  void
  TcpWestwoodWithAgileProbing::PktsAcked(Ptr<TcpSocketState> tcb, uint32_t packetsAcked,
                                         const Time &rtt)
  {
    NS_LOG_FUNCTION(this << tcb << packetsAcked << rtt);
    segment_size = tcb->m_segmentSize;
    ackedSegments += packetsAcked;

    if (rtt.IsZero())
    {
      NS_LOG_WARN("RTT measured is zero!");
      return;
    }
    m_minRtt = std::min(m_minRtt, rtt);

    m_ackedSegments += packetsAcked;
    RE = m_ackedSegments * tcb->m_segmentSize / rtt.GetSeconds();
    double temp = tcb->m_cWnd / m_minRtt.GetSeconds();
    //time interval calculation
    Tk = rtt.GetSeconds() * (temp - RE) / temp;

    cntRtt++;

    Simulator::Schedule(rtt, &TcpWestwoodWithAgileProbing::EstimateBW,
                        this, rtt, tcb);
    if (c == 0)
    {
      if(Tk >0 ){
        Simulator::Schedule(Seconds(Tk), &TcpWestwoodWithAgileProbing::CalculateERE, this);
c++;
      }
    }
    m_ackedSegments = 0;

    // //////std::cout << "tk : " << Tk << " RE: " << RE << " temp : " << temp << " cwnd : " << tcb->m_cWnd << "ssthresh: " << tcb->m_ssThresh << std::endl;
    ////////std::cout << "congestion window :" << tcb->m_cWnd << " ssthtesh :" << tcb->m_ssThresh << std::endl;
  }

  void
  TcpWestwoodWithAgileProbing::Agileprobing(Ptr<TcpSocketState> tcb)
  {
    TracedValue<uint32_t> thresh = ERE * m_minRtt.GetSeconds() / tcb->m_segmentSize;

    tcb->m_ssThresh = std::max(tcb->m_ssThresh, thresh);

    if (tcb->m_cWnd >= tcb->m_ssThresh)
    {
      ////std::cout << " agile congestion window >= ssthresh" << std::endl;
      tcb->m_cWnd += 1;
    }
    else
    {
      ////std::cout << " agile congestion window < ssthresh" << std::endl;
      tcb->m_cWnd += tcb->m_segmentSize;
    }
    restartAP = 0;
  }

  void
  TcpWestwoodWithAgileProbing::PersistentNonCongestionDetection(Ptr<TcpSocketState> tcb)
  {
    double initial_expected_rate = tcb->m_ssThresh / m_minRtt.GetSeconds();
    double expected_rate = (tcb->m_cWnd - (1.5 * tcb->m_segmentSize)) / m_minRtt.GetSeconds();
    TracedValue<double> congestion_boundary = beta * expected_rate +
                                              (1 - beta) * initial_expected_rate;

    if (cntRtt > 2)
    {
      if (RE > congestion_boundary)
      {
        no_congestion_counter++;
      }
      else if (no_congestion_counter > 0)
      {
        no_congestion_counter--;
      }
      if (no_congestion_counter > (tcb->m_cWnd / tcb->m_segmentSize))
      {
        restartAP = 1;
        Agileprobing(tcb);
      }
    }
    else
    {
      no_congestion_counter = 0;
    }
  }

  void
  TcpWestwoodWithAgileProbing::EstimateBW(const Time &rtt, Ptr<TcpSocketState> tcb)
  {
    NS_LOG_FUNCTION(this);

    NS_ASSERT(!rtt.IsZero());

    m_currentBW = m_ackedSegments * tcb->m_segmentSize / rtt.GetSeconds();

    Time currentAck = Simulator::Now();
    m_currentBW = m_ackedSegments * tcb->m_segmentSize / (currentAck - m_lastAck).GetSeconds();
    m_lastAck = currentAck;
    //////std::cout << "currentBW: " << m_currentBW << std::endl;

    NS_LOG_LOGIC("Estimated BW: " << m_currentBW);
  }

  uint32_t
  TcpWestwoodWithAgileProbing::GetSsThresh(Ptr<const TcpSocketState> tcb,
                                           uint32_t bytesInFlight)
  {
    NS_UNUSED(bytesInFlight);
    NS_LOG_LOGIC("CurrentBW: " << m_currentBW << " minRtt: " << tcb->m_minRtt << " ssthresh: " << m_currentBW * static_cast<double>(tcb->m_minRtt.GetSeconds()));

    return std::max(2 * tcb->m_segmentSize,
                    uint32_t(m_currentBW * static_cast<double>(tcb->m_minRtt.GetSeconds())));
  }

  Ptr<TcpCongestionOps>
  TcpWestwoodWithAgileProbing::Fork()
  {
    return CreateObject<TcpWestwoodWithAgileProbing>(*this);
  }

} // namespace ns3
