#ifndef OSMIUM_AREA_DETAIL_PROTO_RING_HPP
#define OSMIUM_AREA_DETAIL_PROTO_RING_HPP

/*

This file is part of Osmium (https://osmcode.org/libosmium).

Copyright 2013-2019 Jochen Topf <jochen@topf.org> and others (see README).

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

#include <osmium/area/detail/node_ref_segment.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/osm/node_ref.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <set>
#include <vector>

namespace osmium {

    class Way;

    namespace area {

        namespace detail {

            /**
             * A ring in the process of being built by the Assembler object.
             */
            class ProtoRing {

            public:

                using segments_type = std::vector<NodeRefSegment*>;

            private:

                // Segments in this ring.
                segments_type m_segments{};

                // If this is an outer ring, these point to it's inner rings
                // (if any).
                std::vector<ProtoRing*> m_inner{};

                // The smallest segment. Will be kept current whenever a new
                // segment is added to the ring.
                NodeRefSegment* m_min_segment;

                // If this is an inner ring, points to the outer ring.
                ProtoRing* m_outer_ring = nullptr;

#ifdef OSMIUM_DEBUG_RING_NO
                static int64_t next_num() noexcept {
                    static int64_t counter = 0;
                    return ++counter;
                }

                int64_t m_num;
#endif

                int64_t m_sum;

            public:

                explicit ProtoRing(NodeRefSegment* segment) noexcept :
                    m_min_segment(segment),
#ifdef OSMIUM_DEBUG_RING_NO
                    m_num(next_num()),
#endif
                    m_sum(0) {
                    add_segment_back(segment);
                }

                void add_segment_back(NodeRefSegment* segment) {
                    assert(segment);
                    if (*segment < *m_min_segment) {
                        m_min_segment = segment;
                    }
                    m_segments.push_back(segment);
                    segment->set_ring(this);
                    m_sum += segment->det();
                }

                NodeRefSegment* min_segment() const noexcept {
                    return m_min_segment;
                }

                ProtoRing* outer_ring() const noexcept {
                    return m_outer_ring;
                }

                void set_outer_ring(ProtoRing* outer_ring) noexcept {
                    assert(outer_ring);
                    assert(m_inner.empty());
                    m_outer_ring = outer_ring;
                }

                const std::vector<ProtoRing*>& inner_rings() const noexcept {
                    return m_inner;
                }

                void add_inner_ring(ProtoRing* ring) {
                    assert(ring);
                    assert(!m_outer_ring);
                    m_inner.push_back(ring);
                }

                bool is_outer() const noexcept {
                    return !m_outer_ring;
                }

                const segments_type& segments() const noexcept {
                    return m_segments;
                }

                const NodeRef& get_node_ref_start() const noexcept {
                    return m_segments.front()->start();
                }

                const NodeRef& get_node_ref_stop() const noexcept {
                    return m_segments.back()->stop();
                }

                bool closed() const noexcept {
                    return get_node_ref_start().location() == get_node_ref_stop().location();
                }

                void reverse() {
                    std::for_each(m_segments.begin(), m_segments.end(), [](NodeRefSegment* segment) {
                        segment->reverse();
                    });
                    std::reverse(m_segments.begin(), m_segments.end());
                    m_sum = -m_sum;
                }

                void mark_direction_done() {
                    std::for_each(m_segments.begin(), m_segments.end(), [](NodeRefSegment* segment) {
                        segment->mark_direction_done();
                    });
                }

                bool is_cw() const noexcept {
                    return m_sum <= 0;
                }

                int64_t sum() const noexcept {
                    return m_sum;
                }

                void fix_direction() noexcept {
                    if (is_cw() == is_outer()) {
                        reverse();
                    }
                }

                void reset() {
                    m_inner.clear();
                    m_outer_ring = nullptr;
                    std::for_each(m_segments.begin(), m_segments.end(), [](NodeRefSegment* segment) {
                        segment->mark_direction_not_done();
                    });
                }

                void get_ways(std::set<const osmium::Way*>& ways) const {
                    for (const auto& segment : m_segments) {
                        ways.insert(segment->way());
                    }
                }

                void join_forward(ProtoRing& other) {
                    m_segments.reserve(m_segments.size() + other.m_segments.size());
                    for (NodeRefSegment* segment : other.m_segments) {
                        add_segment_back(segment);
                    }
                }

                void join_backward(ProtoRing& other) {
                    m_segments.reserve(m_segments.size() + other.m_segments.size());
                    for (auto it = other.m_segments.rbegin(); it != other.m_segments.rend(); ++it) {
                        (*it)->reverse();
                        add_segment_back(*it);
                    }
                }

                void print(std::ostream& out) const {
#ifdef OSMIUM_DEBUG_RING_NO
                    out << "Ring #" << m_num << " [";
#else
                    out << "Ring [";
#endif
                    if (!m_segments.empty()) {
                        out << m_segments.front()->start().ref();
                    }
                    for (const auto& segment : m_segments) {
                        out << ',' << segment->stop().ref();
                    }
                    out << "]-" << (is_outer() ? "OUTER" : "INNER");
                }

            }; // class ProtoRing

            template <typename TChar, typename TTraits>
            inline std::basic_ostream<TChar, TTraits>& operator<<(std::basic_ostream<TChar, TTraits>& out, const ProtoRing& ring) {
                ring.print(out);
                return out;
            }

        } // namespace detail

    } // namespace area

} // namespace osmium

#endif // OSMIUM_AREA_DETAIL_PROTO_RING_HPP
