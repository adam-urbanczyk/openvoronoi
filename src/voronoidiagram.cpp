/* 
 *  Copyright 2010-2011 Anders Wallin (anders.e.e.wallin "at" gmail.com)
 *  
 *  This file is part of OpenVoronoi.
 *
 *  OpenVoronoi is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  OpenVoronoi is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with OpenVoronoi.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cassert>
#include <limits>

#include <boost/foreach.hpp>
#include <boost/math/tools/roots.hpp> // for toms748
#include <boost/current_function.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/assign/list_of.hpp>

#include "voronoidiagram.hpp"
#include "facegrid.hpp"
#include "checker.hpp"
#include "common/numeric.hpp" // for diangle

namespace ovd {

VoronoiDiagram::VoronoiDiagram(double far, unsigned int n_bins) {
    fgrid = new FaceGrid(far, n_bins); // helper-class for nearest-neighbor search 
    vd_checker = new VoronoiDiagramChecker(this); // helper-class that checks topology/geometry
    vpos = new VertexPositioner(this); // helper-class that positions vertices
    far_radius=far;
    initialize();
    num_psites=3;
    num_lsites=0;
    reset_vertex_count();
    debug = false;
}

VoronoiDiagram::~VoronoiDiagram() { 
    delete fgrid; 
    delete vd_checker;
    delete vpos;
}

// add one vertex at origo and three vertices at 'infinity' and their associated edges
void VoronoiDiagram::initialize() {
    using namespace boost::assign;
    double far_multiplier = 6;
    // initial generators/sites:
    Point gen1 = Point( 0, 3.0*far_radius );
    Point gen2 = Point( -3.0*sqrt(3.0)*far_radius/2.0, -3.0*far_radius/2.0 );
    Point gen3 = Point( +3.0*sqrt(3.0)*far_radius/2.0, -3.0*far_radius/2.0 );
    // initial vd-vertices
    Point vd1 = Point(            0                 , -3.0*far_radius*far_multiplier    );
    Point vd2 = Point( +3.0*sqrt(3.0)*far_radius*far_multiplier/2.0, +3.0*far_radius*far_multiplier/2.0);
    Point vd3 = Point( -3.0*sqrt(3.0)*far_radius*far_multiplier/2.0, +3.0*far_radius*far_multiplier/2.0);
    // add init vertices
    HEVertex v00 = g.add_vertex( VoronoiVertex( Point(0,0), UNDECIDED, NORMAL, gen1 ) );
    HEVertex v01 = g.add_vertex( VoronoiVertex( vd1, OUT, OUTER, gen3) );
    HEVertex v02 = g.add_vertex( VoronoiVertex( vd2, OUT, OUTER, gen1) );
    HEVertex v03 = g.add_vertex( VoronoiVertex( vd3, OUT, OUTER, gen2) );
    // add initial sites to graph (if vertex_descriptors not held, why do we do this??)
    g.add_vertex( VoronoiVertex( gen1 , OUT, POINTSITE) );
    g.add_vertex( VoronoiVertex( gen2 , OUT, POINTSITE) );
    g.add_vertex( VoronoiVertex( gen3 , OUT, POINTSITE) );

    // apex-points on the three edges: 
    HEVertex a1 = g.add_vertex( VoronoiVertex( 0.5*(gen2+gen3), UNDECIDED, APEX, gen2 ) );
    HEVertex a2 = g.add_vertex( VoronoiVertex( 0.5*(gen1+gen3), UNDECIDED, APEX, gen3 ) );
    HEVertex a3 = g.add_vertex( VoronoiVertex( 0.5*(gen1+gen2), UNDECIDED, APEX, gen1 ) );

    // add face 1: v0-v1-v2 which encloses gen3
    HEEdge e1_1 =  g.add_edge( v00 , a1 );    
    HEEdge e1_2 =  g.add_edge( a1 , v01 );   
    HEEdge e2   =  g.add_edge( v01, v02 );
    HEEdge e3_1 =  g.add_edge( v02, a2  ); 
    HEEdge e3_2 =  g.add_edge( a2 , v00 ); 
    HEFace f1   =  g.add_face(); 
    g[f1].site  = new PointSite(gen3,f1);
    g[f1].status = NONINCIDENT;
    fgrid->add_face( g[f1] ); // for grid search
    g.set_next_cycle( list_of(e1_1)(e1_2)(e2)(e3_1)(e3_2) , f1 ,1);

    // add face 2: v0-v02-v03 which encloses gen1
    HEEdge e4_1 = g.add_edge( v00, a2  );
    HEEdge e4_2 = g.add_edge( a2, v02 );
    HEEdge e5   = g.add_edge( v02, v03  );
    HEEdge e6_1 = g.add_edge( v03, a3 );
    HEEdge e6_2 = g.add_edge( a3, v00 ); 
    HEFace f2   =  g.add_face();
    g[f2].site  = new PointSite(gen1,f2);
    g[f2].status = NONINCIDENT;    
    fgrid->add_face( g[f2] );
    g.set_next_cycle( list_of(e4_1)(e4_2)(e5)(e6_1)(e6_2) , f2 ,1);

    // add face 3: v0-v3-v1 which encloses gen2
    HEEdge e7_1 = g.add_edge( v00, a3 );  
    HEEdge e7_2 = g.add_edge( a3 , v03 );   
    HEEdge e8   = g.add_edge( v03, v01 );
    HEEdge e9_1 = g.add_edge( v01, a1  ); 
    HEEdge e9_2 = g.add_edge( a1 , v00 ); 
    HEFace f3   =  g.add_face();
    g[f3].site  = new PointSite(gen2,f3); // this constructor needs f3...
    g[f3].status = NONINCIDENT;    
    fgrid->add_face( g[f3] );
    g.set_next_cycle( list_of(e7_1)(e7_2)(e8)(e9_1)(e9_2) , f3 , 1);    

    // set type. (note that edge-params x[8] and y[8] are not set!
    g[e1_1].type = LINE;  g[e1_1].set_parameters(g[f1].site, g[f3].site, false);
    g[e1_2].type = LINE;  g[e1_2].set_parameters(g[f1].site, g[f3].site, true);
    g[e2].type = OUTEDGE; 
    g[e3_1].type = LINE; g[e3_1].set_parameters(g[f2].site, g[f1].site, true);
    g[e3_2].type = LINE; g[e3_2].set_parameters(g[f2].site, g[f1].site, false);
    g[e4_1].type = LINE; g[e4_1].set_parameters(g[f2].site, g[f1].site, false);
    g[e4_2].type = LINE; g[e4_2].set_parameters(g[f2].site, g[f1].site, true);
    g[e5].type = OUTEDGE;
    g[e6_1].type = LINE; g[e6_1].set_parameters(g[f2].site, g[f3].site, false);
    g[e6_2].type = LINE; g[e6_2].set_parameters(g[f2].site, g[f3].site, true);
    g[e7_1].type = LINE; g[e7_1].set_parameters(g[f2].site, g[f3].site, true);
    g[e7_2].type = LINE; g[e7_2].set_parameters(g[f2].site, g[f3].site, false);
    g[e8].type = OUTEDGE;
    g[e9_1].type = LINE; g[e9_1].set_parameters(g[f1].site, g[f3].site, true);
    g[e9_2].type = LINE; g[e9_2].set_parameters(g[f1].site, g[f3].site, false);
    
    // twin edges
    g.twin_edges(e1_1,e9_2);
    g.twin_edges(e1_2,e9_1);
    g[e2].twin = HEEdge(); // the outermost edges have invalid twins
    g[e5].twin = HEEdge();
    g[e8].twin = HEEdge();
    g.twin_edges(e3_1, e4_2);
    g.twin_edges(e3_2, e4_1);
    g.twin_edges(e6_1, e7_2);
    g.twin_edges(e6_2, e7_1);
    
    assert( vd_checker->is_valid() );
}

// comments relate to Sugihara-Iri 1994 paper
// this is roughly "algorithm A" from the paper, page 15/50
//
// 1) find the face that is closest to the new site
// 2) among the vertices on the closest face, find the seed vertex
// 3) grow the tree of IN-vertices
// 4) add new voronoi-vertices on all IN-OUT edges so they becone IN-NEW-OUT
// 5) add new face by splitting each INCIDENT face into two parts by inserting a NEW-NEW edge. 
// 6) remove IN-IN edges and IN-NEW edges
// 7) reset vertex/face status to be ready for next incremental operation
/// insert a point site into the diagram 
/// returns an integer handle to the inserted point. use this integer when inserting lines/arcs
int VoronoiDiagram::insert_point_site(const Point& p, int step) {
    segment_start = HEVertex(); // these are used in find_edge_data(), but only required when inserting line-sites
    segment_end = HEVertex(); // so set them invalid here!
    zero_point_face = g.HFace();
    
    num_psites++;
    int current_step=1;
    assert( p.norm() < far_radius );     // only add vertices within the far_radius circle
    
    HEVertex new_vert = g.add_vertex( VoronoiVertex(p,OUT,POINTSITE) );
    Site* new_site =  new PointSite(p);
    vertex_map.insert( std::pair<int,HEVertex>(g[new_vert].index,new_vert) ); // so that we can find the descriptor later based on its index
    
    HEVertex v_seed = find_seed_vertex( fgrid->grid_find_closest_face( p ), new_site); 
    mark_vertex( v_seed, new_site );
if (step==current_step) return -1; current_step++;
    augment_vertex_set( new_site ); // grow the tree to maximum size
if (step==current_step) return -1; current_step++;
    add_vertices( new_site );  // insert new vertices on IN-OUT edges
if (step==current_step) return -1; current_step++;
    HEFace newface = add_face( new_site );
    g[new_vert].face = newface;
    BOOST_FOREACH( HEFace f, incident_faces ) { // add NEW-NEW edges on all INCIDENT faces
        add_edges(newface, f); // (note: no newface2 parameter given!)
    }
if (step==current_step) return -1; current_step++;
    repair_face( newface );
    if (debug) { std::cout << " new face: "; print_face( newface ); }
    remove_vertex_set(); // remove all IN vertices and adjacent edges
if (step==current_step) return -1; current_step++;
    reset_status(); // reset all vertices to UNDECIDED
    assert( vd_checker->face_ok( newface ) );
    assert( vd_checker->is_valid() );
    return g[new_vert].index;
}

/// insert a line-segment site into the diagram
/// idx1 and idx2 should be int-handles returned from insert_point_site()
bool VoronoiDiagram::insert_line_site(int idx1, int idx2, int step) {
    zero_point_face = g.HFace(); // global. variable is set when a face belonging to a point-site "disappears"
    
    num_lsites++;
    int current_step=1;

    // find the vertices corresponding to idx1 and idx2
    HEVertex start=HEVertex(), end=HEVertex();
    boost::tie(start,end) = find_endpoints(idx1,idx2);
    
    if (debug)
        std::cout << "insert_line_site( " << g[start].index << " - " << g[end].index << " )\n";
    
    // create a point which is left of src->trg
    // determine k (offset-dir) for this point
    // the we know which site/face is the k==+1 and which is k==-1
    Point src_se = g[start].position;
    Point trg_se = g[end  ].position;
    Point left = 0.5*(src_se+trg_se) + (trg_se-src_se).xy_perp(); // this is used below and in find_null_face()
    bool linesite_k_sign = left.is_right(src_se,trg_se); 

    g[start].status=OUT; 
    g[end].status=OUT;     
    g[start].zero_dist();
    g[end].zero_dist();

    Point start_dir = (g[end].position - g[start].position)*(-1);
    HEVertex seg_start, seg_end; // new segment end-point vertices
    HEFace start_null_face, end_null_face; // either existing or new null-faces at endpoints
    
    HEVertex pos_sep_start = HEVertex(); // optional separator endpoints at start
    HEVertex neg_sep_start = HEVertex();
    HEVertex pos_sep_end = HEVertex(); // optional separator endpoints at end
    HEVertex neg_sep_end = HEVertex();

    // returns new seg_start/end vertices, new or existing null-faces, and separator endpoints (if separators should be added)
    boost::tie(seg_start, start_null_face, pos_sep_start, neg_sep_start) = find_null_face(start, end, left);
    boost::tie(seg_end, end_null_face, pos_sep_end, neg_sep_end) = find_null_face(end, start, left);
    
    // these globals are used by repair_face() to avoid taking null-face edges
    null_face1=start_null_face;
    null_face2=end_null_face;
    segment_start = seg_start;
    segment_end = seg_end;
    
    if (debug) {
        std::cout << " start null face : "; print_face(start_null_face);
        std::cout << " end null face : "; print_face(end_null_face);
    }
    HEEdge start_null_edge = g[start_null_face].edge;
    HEEdge   end_null_edge = g[end_null_face  ].edge;
        
    if (step==current_step) {
        std::cout << step << " : startvert=" << g[start].index << " endvert=" << g[end].index << "\n";
        return false; 
    }
    current_step++;
        
    // create positive and negative Site
    // add line-site edges
    // create pos/neg face.
    HEFace pos_face, neg_face;   
    boost::tie(pos_face,neg_face) = add_linesite_edges(seg_start,seg_end,linesite_k_sign); 

    if (debug)
        std::cout << " created pos/neg LineSite \n";

    // the start_face/end_face should belong to the point-site at start/end
    // this is the face where possible separators are inserted later.
    HEFace start_face = find_pointsite_face(start_null_edge); // should be find_seed_face (?) because in some cases there may not be a point-site face!!
    HEFace end_face = find_pointsite_face(end_null_edge);
    
if (step==current_step) return false; current_step++;
    if (debug)
        std::cout << " start/end face = " << start_face << " " << end_face << "\n";
        
    // on the face of start-point, find the seed vertex
    HEVertex v_seed = find_seed_vertex(start_face, g[pos_face].site ) ;
    if (debug)
        std::cout << " start face seed  = " << g[v_seed].index << "\n";
    mark_vertex( v_seed, g[pos_face].site  );

    // now safe to set the zero-face edge
    // in the collinear case, set the edge for the
    if (zero_point_face!=g.HFace())
        g[zero_point_face].edge = start_null_edge;
    
if (step==current_step) return false; current_step++;

    augment_vertex_set( g[pos_face].site  ); // it should not matter if we use pos_site or neg_site here
    // todo(?) sanity checks:
    // check that end_face is INCIDENT? 
    // check that tree (i.e. v0) includes end_face_seed ?
    if (debug) {
        std::cout << " delete-set |v0|="<< v0.size() <<" : "; 
        print_vertices(v0);
    }
    
if (step==current_step) return false; current_step++;

    add_vertices( g[pos_face].site );  // add new vertices on all IN-OUT edges.

if (step==current_step) return false; current_step++;
    
    typedef boost::tuple<HEEdge, HEVertex, HEEdge,bool> SepTarget;
    SepTarget pos_start_target, neg_start_target;
    
    // find the pos targets first
    pos_start_target = find_separator_target(start_face, pos_sep_start);
    neg_start_target = find_separator_target(start_face, neg_sep_start);
    
    // then add the separators
    add_separator(start_face, start_null_face, pos_start_target, pos_sep_start, g[pos_face].site , g[neg_face].site );
    
if (step==current_step) return false; current_step++;
        
    add_separator(start_face, start_null_face, neg_start_target, neg_sep_start, g[pos_face].site , g[neg_face].site );
    g[start_face].status = NONINCIDENT; // face is now done.
    assert( vd_checker->face_ok( start_face ) );
    
if (step==current_step) return false; current_step++;

    SepTarget pos_end_target, neg_end_target;
    pos_end_target = find_separator_target(end_face, pos_sep_end);
    neg_end_target = find_separator_target(end_face, neg_sep_end);
    add_separator(end_face, end_null_face, pos_end_target, pos_sep_end, g[pos_face].site , g[neg_face].site );
    
if (step==current_step) return false; current_step++;
    
    add_separator(end_face, end_null_face, neg_end_target, neg_sep_end, g[pos_face].site , g[neg_face].site );

    g[end_face].status = NONINCIDENT;
    assert( vd_checker->face_ok( end_face ) );

    if(debug) std::cout << " all separators  done.\n";

if (step==current_step) return false; current_step++;

    if(debug) std::cout << " adding edges.\n";
    BOOST_FOREACH( HEFace f, incident_faces ) {
        if ( g[f].status == INCIDENT )  {// end-point faces already dealt with in add_separator()
            if(debug) { 
                std::cout << " add_edges f= " << f << "\n";
                print_face(f);
            }
            add_edges( pos_face, f, neg_face); // each INCIDENT face is split into two parts: newface and f
        }
    }

if (step==current_step) return false; current_step++;

    remove_vertex_set();

    if (debug) { std::cout << "will now repair pos/neg faces: " << pos_face << " " << neg_face << "\n"; }

    repair_face( pos_face );
    assert( vd_checker->face_ok( pos_face ) );

    repair_face( neg_face );
    assert( vd_checker->face_ok( neg_face ) );

if (step==current_step) return false; current_step++;

    // we are done and can remove split-vertices
    BOOST_FOREACH(HEFace f, incident_faces) {
        remove_split_vertex(f);
    }
    reset_status();
    
    if (debug) {
        std::cout << "faces " << start_face << " " << end_face << " " << pos_face << " " << neg_face << " repaired \n";
        std::cout << "insert_line_site(" << g[start].index << "-"<< g[end].index << ") done.\n";
        std::cout << " start_face "; print_face(start_face);
        std::cout << " start_null_face "; print_face(start_null_face);
        std::cout << " end_face "; print_face(end_face);
        std::cout << " end_null_face "; print_face(end_null_face);
        std::cout << " pos_face "; print_face(pos_face);
        std::cout << " neg_face "; print_face(neg_face);
    }
    assert( vd_checker->face_ok( start_face ) );
    assert( vd_checker->face_ok( start_null_face ) );
    assert( vd_checker->face_ok( end_face ) );
    assert( vd_checker->face_ok( end_null_face ) );
    assert( vd_checker->face_ok( pos_face ) );
    assert( vd_checker->face_ok( neg_face ) );    
    assert( vd_checker->is_valid() );
    return true; 
}

// iterate around the (null)face to find the an edge belonging to a point-site
HEFace VoronoiDiagram::find_pointsite_face(HEEdge start_edge) {
    HEEdge current_e = start_edge;
    HEEdge start_e = current_e;
    HEFace start_face  = g[ g[start_edge].twin ].face;
    do {
        HEEdge twin = g[current_e].twin;
        HEFace twin_f = g[twin].face;
        Site* site = g[twin_f].site;
        if (site) {
            if (site->isPoint()) {
                start_face = twin_f;
            }
        }
        current_e = g[current_e].next;
    } while (current_e!=start_e);
    return start_face;
}

std::pair<HEFace,HEFace> VoronoiDiagram::add_linesite_edges(
                                                 HEVertex seg_start, HEVertex seg_end,bool linesite_k_sign) 
{
    HEFace pos_face, neg_face;  
    LineSite* pos_site;
    LineSite* neg_site;
    HEEdge pos_edge, neg_edge;
    if ( linesite_k_sign ) {
        pos_site = new LineSite( g[seg_start].position, g[seg_end  ].position , +1);
        neg_site = new LineSite( g[seg_end  ].position, g[seg_start].position , -1);
        boost::tie(pos_edge,neg_edge) = g.add_twin_edges( seg_start,   seg_end );
    } else {
        pos_site = new LineSite( g[seg_end  ].position, g[seg_start].position , +1);
        neg_site = new LineSite( g[seg_start].position, g[seg_end  ].position , -1);
        boost::tie( pos_edge, neg_edge) = g.add_twin_edges( seg_end  ,seg_start );
    }
    g[pos_edge].type = LINESITE;
    g[neg_edge].type = LINESITE;
    g[pos_edge].k = +1;
    g[neg_edge].k = -1;
    //g.twin_edges(pos_edge, neg_edge);
    assert( vd_checker->check_edge(pos_edge) && vd_checker->check_edge(neg_edge) );
    pos_face = add_face( pos_site ); //  this face to the left of start->end edge    
    neg_face = add_face( neg_site ); //  this face is to the left of end->start edge
    g[pos_face].edge = pos_edge;
    g[neg_face].edge = neg_edge;
    g[pos_edge].face = pos_face;
    g[neg_edge].face = neg_face;    
    return std::make_pair(pos_face,neg_face);
}

// given indices idx1 and idx2, return the corresponding vertex descriptors
// the vertex_map is populated in insert_point_site()
std::pair<HEVertex,HEVertex> VoronoiDiagram::find_endpoints(int idx1, int idx2) {
    std::map<int,HEVertex>::iterator it_start, it_end;
    it_start = vertex_map.find(idx1);
    it_end = vertex_map.find(idx2);
    assert( it_start != vertex_map.end() && it_end != vertex_map.end() ); // we must find idx1 and idx2 in the map
    return std::make_pair( it_start->second, it_end->second);
}

// on a face, search and return the left/right edge from endp
std::pair<HEEdge,HEEdge> VoronoiDiagram::find_next_prev(HEFace null_face, HEVertex endp) {
    HEEdge current = g[null_face].edge;
    HEEdge start_edge = current;
    HEEdge next_edge = HEEdge();
    HEEdge prev_edge = HEEdge();
    do {
        HEVertex src = g.source(current);
        HEVertex trg = g.target(current);
        if (src==endp)
            next_edge = current;
        if (trg==endp)
            prev_edge = current;
            
        current = g[current].next;
    } while (current!=start_edge);
    assert( next_edge != HEEdge() );
    assert( prev_edge != HEEdge() );
    if (debug) {
        std::cout << " find_next_prev() next_edge = "; print_edge(next_edge); 
        std::cout << " find_next_prev() prev_edge = "; print_edge(prev_edge);
    }
    return std::make_pair(next_edge, prev_edge);
}

// check if next_edge is an OK edge for the null face.
// it is OK if either:
// - the target is an ENDPOINT
// - the target alfa is larger than the separator.alfa for seg_start
HEVertex VoronoiDiagram::process_next_null(Point dir, HEEdge next_edge , bool k3) {
    HEVertex trg = g.target(next_edge);
    HEVertex src = g.source(next_edge);
    assert( g[src].type == ENDPOINT );
    HEVertex sep_point = HEVertex();
    Point neg_sep_dir = dir.xy_perp(); // call this pos?
    double neg_sep_alfa = numeric::diangle(neg_sep_dir.x,neg_sep_dir.y);
    if (debug) {
        std::cout << "process_next_null() e="; print_edge(next_edge);
    }
    if ( g[trg].type == ENDPOINT ) { // target is endpoint
        // if we have an ENDPOINT then there should not be room for a separator
        if ( numeric::diangle_bracket( g[src].alfa, neg_sep_alfa, g[trg].alfa ) ) {
            assert(0);
        }
        // insert a normal vertex, positioned at mid-alfa between src/trg.
        HEVertex new_v = g.add_vertex( VoronoiVertex(g[src].position,NEW,NORMAL,g[src].position) );
        double mid = numeric::diangle_mid( g[src].alfa, g[trg].alfa  );
        g[new_v].alfa = mid;
        modified_vertices.insert(new_v);
        g.add_vertex_in_edge( new_v, next_edge);
        if (k3)
            g[new_v].k3=+1;
        else
            g[new_v].k3=-1;

        if (debug) {
            std::cout << " e.trg=(ENDPOINT) \n";
            std::cout << " added NEW NORMAL vertex " << g[new_v].index << " in edge "; print_edge(next_edge);
        }
    } else {
        if ( neg_sep_alfa == g[trg].alfa && g[trg].type == SEPPOINT ) {
            std::cout << " identical SEPPOINT case!\n";
            // assign face of separator-edge
            // mark separator target NEW
            HEEdge sep_edge=HEEdge();
            BOOST_FOREACH(HEEdge e, g.out_edge_itr(trg) ) {
                assert( g.source(e) == trg );
                if ( g[e].type == SEPARATOR )
                    sep_edge = e;
            }
            assert(sep_edge!=HEEdge()); 
            if (debug) { 
                std::cout << " existing SEPARATOR is "; print_edge(sep_edge);
            }
            HEEdge sep_twin = g[sep_edge].twin;
            HEFace sep_face =  g[sep_edge].face;
            Site* sep_site = g[sep_face].site;
            HEFace sep_twin_face = g[sep_twin].face;
            Site* sep_twin_site = g[sep_twin_face].site;
            HEEdge pointsite_edge;
            if (sep_site->isPoint() ) {
                std::cout << " PointSite SEPARATOR is "; print_edge(sep_edge);
                pointsite_edge = sep_edge;
            }
            if (sep_twin_site->isPoint() ) {
                std::cout << " PointSite SEPARATOR is "; print_edge(sep_twin);
                pointsite_edge = sep_twin;
            }
            // set face for pointsite_edge ?
            zero_point_face = g[pointsite_edge].face; // this face will be removed/contracted.
            //g[zero_point_face].edge = next_edge; // this interferes with find_seed!
            
            // set the separator target to NEW
            HEVertex sep_target = g.target(sep_edge);
            g[sep_target].status = NEW;
            modified_vertices.insert(sep_target);
            
            return HEVertex(); // no new separator-point returned
        }
            
        // not ENDPOINT. add SEPPOINT if there is room, and PointSite
        HEFace next_face = g[ g[next_edge].twin ].face;
        Site* next_edge_site = g[next_face].site;
        
        if ( numeric::diangle_bracket( g[src].alfa, neg_sep_alfa, g[trg].alfa ) && next_edge_site->isPoint() ) {
            if (debug) {
                std::cout << " inserting SEPPOINT in edge: "; print_edge(next_edge);
            }
            sep_point = add_sep_point(src, next_edge, neg_sep_dir);
            if (k3)
                g[sep_point].k3=+1;
            else
                g[sep_point].k3=-1;

        } else {
            // target is not endpoint, and no room for separator, so we push it and convert it
            HEEdge next_next = g[next_edge].next;
            HEVertex next_trg = g.target(next_next);
            double mid = numeric::diangle_mid( g[src].alfa, g[next_trg].alfa  );
            
            if ( numeric::diangle_bracket(neg_sep_alfa, mid  , g[next_trg].alfa ) ) {
                // if mid is beyond the separator-position, the pushed vertex becomes a SEPPOINT
                if (debug) std::cout << " pushed vertex " << g[trg].index << " becomes SEPPOINT\n";
                g[trg].alfa = neg_sep_alfa;
                g[trg].type = SEPPOINT;
                g[trg].status = NEW;
                sep_point = trg; // this is the return value of this function!
            } else {
                // otherwise it becomes a nromal NEW vertex
                if (debug) std::cout << " pushed vertex " << g[trg].index << " becomes NORMAL\n";
                g[trg].alfa = mid;
                g[trg].type = NORMAL;
                g[trg].status = NEW;
            }
            if (k3)
                g[trg].k3=+1;
            else
                g[trg].k3=-1;

            modified_vertices.insert(trg);
            

        }
    }
    return sep_point;
}

HEVertex VoronoiDiagram::process_prev_null(Point dir, HEEdge prev_edge , bool k3) {
    HEVertex trg = g.target(prev_edge);
    HEVertex src = g.source(prev_edge); // prev_edge is on null-face !
    assert( g[trg].type == ENDPOINT );
    
    HEVertex sep_point = HEVertex();
    Point pos_sep_dir = dir.xy_perp()*(-1);  // call this neg?
    double pos_sep_alfa = numeric::diangle(pos_sep_dir.x,pos_sep_dir.y);
    if (debug) {
        std::cout << "process_prev_null() e="; print_edge(prev_edge);
    }
        
    if ( g[src].type == ENDPOINT ) { //  endpoint

        if ( numeric::diangle_bracket( g[src].alfa, pos_sep_alfa, g[trg].alfa ) ) {
            std::cout << " src " << g[src].index << " a= " << g[src].alfa << "\n"; 
            std::cout << " trg " << g[trg].index << " a= " << g[trg].alfa << "\n"; 
            std::cout << " pos_sep_alfa =  " << pos_sep_alfa << "\n";
            
            assert(0); 
            // will this ever happen??   NEW_ENDP -> ENDP  AND we have room for a separator?
        }
        // insert normal NEW vertex here!
        HEVertex new_v = g.add_vertex( VoronoiVertex(g[src].position,NEW,NORMAL) );
        double mid = numeric::diangle_mid( g[src].alfa, g[trg].alfa  );
        g[new_v].alfa = mid;
        modified_vertices.insert(new_v);
        g.add_vertex_in_edge(new_v,prev_edge);
        if (k3)
            g[new_v].k3=-1;
        else
            g[new_v].k3=+1;

        if (debug) {
            std::cout << " added NEW NORMAL vertex " << g[new_v].index << " in edge ";
            print_edge(prev_edge);
        }
    } else {
        // we are not dealing with an ENDPOINT
        
        if ( pos_sep_alfa == g[src].alfa && g[src].type == SEPPOINT ) {
            std::cout << " identical SEPPOINT case!\n";
            
            // assign face of separator-edge
            // mark separator target NEW
            HEEdge sep_edge=HEEdge();
            BOOST_FOREACH(HEEdge e, g.out_edge_itr(src) ) {
                assert( g.source(e) == src );
                if ( g[e].type == SEPARATOR )
                    sep_edge = e;
            }
            assert(sep_edge!=HEEdge()); 
            if (debug) { 
                std::cout << " existing SEPARATOR is "; print_edge(sep_edge);
            }
            HEEdge sep_twin = g[sep_edge].twin;
            HEFace sep_face =  g[sep_edge].face;
            Site* sep_site = g[sep_face].site;
            HEFace sep_twin_face = g[sep_twin].face;
            Site* sep_twin_site = g[sep_twin_face].site;
            HEEdge pointsite_edge;
            if (sep_site->isPoint() ) {
                std::cout << " PointSite SEPARATOR is "; print_edge(sep_edge);
                pointsite_edge = sep_edge;
            }
            if (sep_twin_site->isPoint() ) {
                std::cout << " PointSite SEPARATOR is "; print_edge(sep_twin);
                pointsite_edge = sep_twin;
            }
            // set face for pointsite_edge to the null_face ?
            zero_point_face = g[pointsite_edge].face; // this face will be removed/contracted.
            //g[zero_point_face].edge = prev_edge;
            
            // set the separator target to NEW
            HEVertex sep_target = g.target(sep_edge);
            g[sep_target].status = NEW;
            modified_vertices.insert(sep_target);
            
            return sep_point;
        }
        
        if ( numeric::diangle_bracket( g[src].alfa, pos_sep_alfa, g[trg].alfa ) ) {
            if (debug) {
                std::cout << " inserting SEPPOINT in edge: "; print_edge(prev_edge);
            }
            sep_point = add_sep_point(src, prev_edge, pos_sep_dir);
            if (k3)
                g[sep_point].k3=-1;
            else
                g[sep_point].k3=+1;

        } else {
            // target is not ENDPOINT, and no room for SEPPOINT, so we push it and convert it
            HEEdge prev_prev = g.previous_edge(prev_edge);
            HEVertex prev_src = g.source(prev_prev);
            double mid = numeric::diangle_mid( g[prev_src].alfa, g[trg].alfa  );
 
            // if mid is beyond the separator-position, the pushed vertex becomes a SEPPOINT
            // otherwise it becomes a nromal NEW vertex
            if ( numeric::diangle_bracket(mid,  pos_sep_alfa, g[trg].alfa ) ) {
                if (debug) std::cout << " pushed vertex " << g[src].index << " becomes SEPPOINT\n";
                g[src].alfa = pos_sep_alfa;
                g[src].type = SEPPOINT;
                g[src].status = NEW;
                sep_point = src; // this is the return value of this function!
            } else {
                if (debug) std::cout << " pushed vertex " << g[src].index << " becomes NORMAL\n";
                g[src].alfa = mid;
                g[src].type = NORMAL;
                g[src].status = NEW;
            }
            if (k3)
                g[src].k3=-1;
            else
                g[src].k3=+1;
            modified_vertices.insert(src);
        }
    }
    return sep_point;
}

HEVertex VoronoiDiagram::add_sep_point(HEVertex endp, HEEdge edge, Point sep_dir) {
    HEVertex sep = g.add_vertex( VoronoiVertex(g[endp].position,OUT,SEPPOINT) );
    g[sep].set_alfa(sep_dir);
    if (debug) {
        std::cout << " adding separator " << g[sep].index << " in null edge "; 
        print_edge(edge);
    }
    g.add_vertex_in_edge(sep,edge);
    return sep;
}

// either find an existing null-face, or create a new one.
//
// return segment-endpoint and separator-points,
boost::tuple<HEVertex,HEFace,HEVertex,HEVertex>
VoronoiDiagram::find_null_face(HEVertex start, HEVertex other, Point left) {
    HEVertex seg_start = HEVertex(); // new end-point vertices
    HEFace start_null_face; // either existing or new null-faces at endpoints
    HEVertex pos_sep_start = HEVertex(); // optional separator endpoints at start
    HEVertex neg_sep_start = HEVertex();
    Point dir = g[other].position - g[start].position;
    double alfa = numeric::diangle( dir.x, dir.y );
    bool k3_sign = left.is_right( g[start].position , g[other].position); // this is used below and in find_null_face()
        
    if (g[start].null_face != g.HFace() ) {
        // there is an existing null face
        if (debug) std::cout << " find_null_face() endp= " << g[start].index << " has existing null_face : " << g[start].null_face << "\n";
        start_null_face = g[start].null_face;
        
        // create segment endpoint
        seg_start = g.add_vertex( VoronoiVertex(g[start].position,OUT,ENDPOINT) );
        g[seg_start].zero_dist();
        // find the edge on the null-face where we insert seg_start
        HEEdge current = g[start_null_face].edge;
        HEEdge start_edge = current;
        
        g[seg_start].set_alfa(dir);
        HEEdge insert_edge= HEEdge();
        bool found = false;
        do {
            HEVertex src = g.source(current);
            HEVertex trg = g.target(current);
            if ( numeric::diangle_bracket( g[src].alfa, alfa, g[trg].alfa ) ) {
                    insert_edge = current;
                    found = true;
            }
            current = g[current].next;
        } while (current!=start_edge && !found);
        assert( insert_edge != HEEdge() );
        // insert endpoint in null-edge
        g.add_vertex_in_edge(seg_start,insert_edge);
        
        if (debug) {
            std::cout << "  null_face before sep/new vertices:\n";
            std::cout << "  "; print_face(start_null_face);
        }
        // delete/contract everything until separator.alfa OR endpoint reached
        
        HEEdge next_edge, prev_edge;
        boost::tie(next_edge,prev_edge) = find_next_prev(start_null_face, seg_start);
        assert( g[prev_edge].next == next_edge );
        neg_sep_start = process_next_null(dir,next_edge, k3_sign);
        pos_sep_start = process_prev_null(dir,prev_edge, k3_sign);

    } else {
        // create a new null face at start
        //
        //  neg_sep -> seg_endp -> pos_sep
        //
        start_null_face = g.add_face(); //  this face to the left of start->end edge    
        if (debug) std::cout << " find_null_face() endp= " << g[start].index <<  " creating new null_face " << start_null_face << "\n";
        seg_start = g.add_vertex( VoronoiVertex(g[start].position,OUT,ENDPOINT) );
        g[seg_start].zero_dist();
        g[seg_start].set_alfa(dir);
        g[seg_start].k3=0;
        pos_sep_start = g.add_vertex( VoronoiVertex(g[start].position,OUT,SEPPOINT) );
        neg_sep_start = g.add_vertex( VoronoiVertex(g[start].position,OUT,SEPPOINT) );
        
        g[pos_sep_start].zero_dist();
        g[neg_sep_start].zero_dist();
    
        if (k3_sign) {
            g[pos_sep_start].k3 = +1; 
            g[neg_sep_start].k3 = -1;
        } else {            
            g[pos_sep_start].k3 = -1; 
            g[neg_sep_start].k3 = +1;
        }
        
        g[pos_sep_start].set_alfa( dir.xy_perp()*(+1) );        
        g[neg_sep_start].set_alfa( dir.xy_perp()*(-1) );
        
        if (debug) {
            std::cout << " k3_sign = " << k3_sign <<"\n"; 
            std::cout << " sep1 = " << g[pos_sep_start].index << " k3=" << g[pos_sep_start].k3 << "\n";
            std::cout << " sep2 = " << g[neg_sep_start].index << " k3=" << g[neg_sep_start].k3 << "\n";
        }
        
        HEEdge e1,e1_tw;
        boost::tie(e1,e1_tw) = g.add_twin_edges(seg_start,pos_sep_start);
        HEEdge e2,e2_tw;
        boost::tie(e2,e2_tw) = g.add_twin_edges(pos_sep_start,neg_sep_start);
        HEEdge e3,e3_tw;
        boost::tie(e3,e3_tw) = g.add_twin_edges(neg_sep_start,seg_start);
        
        // e1  ->  e2  ->  e3     on start_null_face, k=1
        // e1t <-  e2t <-  e3t   on g[start].face, k=1
        g.set_next_cycle( boost::assign::list_of(e1)(e2)(e3) , start_null_face, 1);
        HEFace start_face=g[start].face;
        HEEdge start_face_edge=g[start_face].edge; // crazy workaround, because set_next_cycles sets g[face].edge wrong here!
        g.set_next_cycle( boost::assign::list_of(e3_tw)(e2_tw)(e1_tw), g[start].face, 1);
        g[start_null_face].edge = e1;
        g[start_face].edge=start_face_edge;
        
        g[e1].type = NULLEDGE; g[e2].type = NULLEDGE; g[e3].type = NULLEDGE;
        g[e1_tw].type = NULLEDGE; g[e3_tw].type = NULLEDGE; g[e2_tw].type = NULLEDGE;

        g[start].null_face = start_null_face;
    }
    return boost::make_tuple(seg_start,start_null_face,pos_sep_start,neg_sep_start);
}

/// add separator on the face f, which contains the endpoint
/// f is the face of endp 
/// s1 and s2 are the pos and neg LineSites
void VoronoiDiagram::add_separator(HEFace f, HEFace null_face, 
                                   boost::tuple<HEEdge, HEVertex, HEEdge,bool> target,
                                   HEVertex sep_endp, Site* s1, Site* s2) {
    if ( sep_endp == HEVertex() ) // no separator
        return; // do nothing!
    
    if (debug) {
        std::cout << "add_separator() f="<<f<<" endp=" << g[sep_endp].index << "\n";
    }
    assert( (g[sep_endp].k3==1) || (g[sep_endp].k3==-1) );    
    
    HEEdge endp_next = HEEdge();
    HEEdge endp_prev = HEEdge();
    HEEdge endp_next_tw = HEEdge();
    HEEdge endp_prev_tw = HEEdge();

    boost::tie( endp_next_tw, endp_prev_tw ) = find_next_prev(null_face, sep_endp);
    endp_prev = g[endp_next_tw].twin; // NOTE twin!
    endp_next = g[endp_prev_tw].twin; // NOTE twin!
    assert( endp_next != HEEdge() );
    assert( endp_prev != HEEdge() );

    // find NEW vertex on the old face f
    // this vertex has the correct alfa angle for this endp/separator
    HEEdge v_previous = boost::get<0>(target);
    HEVertex v_target = boost::get<1>(target);
    HEEdge    v_next  = boost::get<2>(target);
    bool out_new_in   = boost::get<3>(target);

    assert( (g[v_target].k3==1) || (g[v_target].k3==-1) );    
    assert( g[sep_endp].k3 == g[v_target].k3 );
    assert( s1->in_region( g[v_target].position ) ); // v1 and v2 should be in the region of the line-site
    assert( s2->in_region( g[v_target].position ) );
    
    // add new separator edge, and its twin
    HEEdge e2, e2_tw;
    boost::tie(e2,e2_tw) = g.add_twin_edges( sep_endp, v_target );
    g[e2].type    = SEPARATOR;
    g[e2_tw].type = SEPARATOR;
    
    // there are two cases. depending on how v_target (NEW) is found:
    // OUT-NEW-IN, when out_new_in = true
    // IN-NEW-OUT, when out_new_in = false
    // here we set the faces, sites, and next-pointers depending on the case
    if ( out_new_in ) {
        g[e2].k    = g[v_target].k3; // e2 is on the segment side
        g[e2_tw].k = +1;             // e2_tw is on the point-site side
        g[e2_tw].face = f;
        g[f].edge = e2_tw;
        g[endp_prev].k = g[e2].k; // endp_prev is on the line-site side

        if (g[e2].k == -1) { // choose either s1 or s2 as the site
            g[e2].face = s2->face;
            g[s2->face].edge=e2;
            g[endp_prev].face = s2->face;
        } else {
            g[e2].face = s1->face;
            g[s1->face].edge=e2;
            g[endp_prev].face = s1->face;
        }

        g.set_next(v_previous,e2_tw);
        g.set_next(e2_tw, endp_next);

        g[endp_next].face = f;      // the null-edge
        g[endp_next].k = 1;

        //g.set_next(endp_prev,e2); // don't set!

        g.set_next(e2,v_next);
    } else {
        g[e2].k    = +1;             // e2 is on the point-site side
        g[e2_tw].k = g[v_target].k3; // e2_tw is on the segment side
        g[e2].face    = f;
        g[f].edge     = e2;
        g[endp_next].k = g[e2_tw].k; // endp_next is on the linesite-side
        if (g[e2_tw].k == -1) {
            g[e2_tw].face = s2->face;
            g[s2->face].edge=e2_tw;
            g[endp_next].face = s2->face;
        } else {
            g[e2_tw].face = s1->face;
            g[s1->face].edge=e2_tw;
            g[endp_next].face = s1->face;
        }
        g.set_next(v_previous,e2_tw);
        //g.set_next(e2_tw,endp_next);
        g[endp_prev].face = f;
        g[endp_prev].k = 1;

        g.set_next(endp_prev, e2);
        g.set_next(e2,v_next);
    }
    g[e2   ].set_sep_parameters( g[sep_endp].position, g[v_target].position );
    g[e2_tw].set_sep_parameters( g[sep_endp].position, g[v_target].position );
        
    if (debug) {
        std::cout << "added separator: ";
        std::cout << g[sep_endp].index << " - " << g[v_target].index << "\n";
    }
    assert( vd_checker->check_edge(e2) );
    assert( vd_checker->check_edge(e2_tw) );
}

/// find amount of clearance-disk violation on all face vertices 
/// return vertex with the largest violation
HEVertex VoronoiDiagram::find_seed_vertex(HEFace f, Site* site)  {
    if (debug) { std::cout << "find_seed_vertex on f=" << f << "\n"; print_face(f); }
    double minPred( 0.0 ); 
    HEVertex minimalVertex = HEVertex();
    bool first( true );
    HEEdge current = g[f].edge;
    HEEdge start = current;
    do {        
        HEVertex q = g.target(current);
        if ( (g[q].status != OUT) && (g[q].type == NORMAL) ) {
            double h = g[q].in_circle( site->apex_point( g[q].position ) ); 
            if ( first || ( (h<minPred) && (site->in_region(g[q].position) ) ) ) {
                minPred = h;
                minimalVertex = q;
                first = false;
            }
        }
        current = g[current].next;
    } while(current!=start);  
    assert( minPred < 0 );
    return minimalVertex;
}


// growing the v0/delete-tree of "IN" vertices by "weighted breadth-first search"
// we start at the seed and add vertices with detH<0 provided that:
// (C4) v should not be adjacent to two or more IN vertices (this would result in a loop/cycle!)
// (C5) for an incident face containing v: v is adjacent to an IN vertex on this face
// C4 and C5 refer to the Sugihara&Iri 1992 "one million" paper 
//  we process UNDECIDED vertices adjacent to known IN-vertices in a "weighted breadth-first-search" manner
//  where vertices with a large fabs(detH) are processed first, since we assume the in-circle predicate
//  to be more reliable the larger fabs(in_circle()) is.
void VoronoiDiagram::augment_vertex_set(  Site* site ) {
    while( !vertexQueue.empty() ) {
        HEVertex v = HEVertex();
        double h(0);
        boost::tie( v, h ) = vertexQueue.top();
        assert( g.g[v].status == UNDECIDED );
        vertexQueue.pop(); 
        if ( h < 0.0 ) { // try to mark IN if h<0 and passes (C4) and (C5) tests and in_region(). otherwise mark OUT
            if ( predicate_c4(v) || !predicate_c5(v) || !site->in_region(g[v].position) ) {
                g[v].status = OUT; // C4 or C5 violated, so mark OUT
                if (debug) std::cout << g[v].index << " marked OUT (topo): c4="<< predicate_c4(v) << " c5=" << !predicate_c5(v) << " r=" << !site->in_region(g[v].position) << " h=" << h << "\n";
            } else {
                mark_vertex( v,  site); // h<0 and no violations, so mark IN. push adjacent UNDECIDED vertices onto Q.
                if (debug) std::cout << g[v].index << " marked IN (in_circle) ( " << h << " )\n";
            }
        } else { 
            g[v].status = OUT; // detH was positive (or zero), so mark OUT
            if (debug) std::cout << g[v].index << " marked OUT (in_circle) ( " << h << " )\n";
        }
        modified_vertices.insert( v );
    }
    
    assert( vertexQueue.empty() );
    if (debug) std::cout << "augment_vertex_set() DONE\n";

    // sanity-check?: for all incident faces the IN/OUT-vertices should be connected
}

// mark vertex IN. mark adjacent faces INCIDENT
// push adjacent UNDECIDED vertices onto queue 
void VoronoiDiagram::mark_vertex(HEVertex& v,  Site* site) {
    g[v].status = IN;
    v0.push_back( v );
    modified_vertices.insert(v);
    
    if (site->isPoint())
        mark_adjacent_faces_p(v,site);
    else
        mark_adjacent_faces( v, site );

    // push the v-adjacent vertices onto the queue
    BOOST_FOREACH(HEEdge e, g.out_edge_itr( v )) {
        HEVertex w = g.target( e );
        if ( (g[w].status == UNDECIDED) && (!g[w].in_queue) ) {
                // when pushing onto queue we also evaluate in_circle predicate so that we process vertices in the correct order
                vertexQueue.push( VertexDetPair(w , g[w].in_circle(site->apex_point(g[w].position)) ) ); 
                g[w].in_queue=true;
                if (debug) std::cout << "  " << g[w].index << " queued \n";
        }
    }
}

// IN-Vertex v has three adjacent faces, mark nonincident faces incident
// and push them to the incident_faces queue
// NOTE: call this only when inserting point-sites
void VoronoiDiagram::mark_adjacent_faces_p( HEVertex v, Site* site) {
    assert( g[v].status == IN );
    BOOST_FOREACH(HEEdge e, g.out_edge_itr( v )) {
        HEFace adj_face = g[e].face;
        if ( g[adj_face].status  != INCIDENT ) {
            g[adj_face].status = INCIDENT; 
            incident_faces.push_back(adj_face);
        }
    }

}

// call this when inserting line-sites
// since we call add_split_vertex we can't use iterators, because they get invalidated
// so use the slower adjacent_faces() instead.
void VoronoiDiagram::mark_adjacent_faces( HEVertex v, Site* site) {
    assert( g[v].status == IN );
    FaceVector new_adjacent_faces = g.adjacent_faces( v );
    
    assert(
        (g[v].type == APEX && new_adjacent_faces.size()==2 ) ||
        (g[v].type == SPLIT && new_adjacent_faces.size()==2 ) ||
        new_adjacent_faces.size()==3
    );

    BOOST_FOREACH( HEFace adj_face, new_adjacent_faces ) {
        if ( g[adj_face].status != INCIDENT ) {
            if ( site->isLine() )
                add_split_vertex(adj_face, site);

            g[adj_face].status = INCIDENT;
            incident_faces.push_back(adj_face);
        }
    }
}


// walk around the face f
// return edges whose endpoints are on separate sides of pt1-pt2 line
// FIXME/todo ?not all edges found like this *need* SPLIT vertices? (but it does not hurt to insert SPLIT-vertices in this case)
EdgeVector VoronoiDiagram::find_split_edges(HEFace f, Point pt1, Point pt2) {
    assert( vd_checker->face_ok(f) );
    EdgeVector out;
    HEEdge current_edge = g[f].edge;
    HEEdge start_edge = current_edge;
    //int count=0;                             
    do { // FIND ALL! not just one.
        HEVertex trg = g.target( current_edge );
        HEVertex src = g.source( current_edge );
        bool src_is_right = g[src].position.is_right(pt1,pt2);
        bool trg_is_right = g[trg].position.is_right(pt1,pt2);
        if ( g[src].type == NORMAL || g[src].type == APEX || g[src].type == SPLIT) { //? check edge-type instead?
            if ( src_is_right != trg_is_right  ) { 
                out.push_back(current_edge);
                assert(vd_checker->check_edge(current_edge));
            }
        }
        current_edge = g[current_edge].next;   
        //count++;
        //assert(count<100000); // some reasonable max number of edges in face, to avoid infinite loop
    } while (current_edge!=start_edge);
    
    if (debug) {
        std::cout << " face " << f << " requires SPLIT vertices on edges: \n";
        BOOST_FOREACH( HEEdge e, out ) {
            std::cout << "  "; print_edge(e);
        }
    }
    return out;
}

// add one or many split-vertices to the edges of the give face
//
// these are projections/mirrors of the site of f with the new Site s acting as the mirror
//
// split edges are inserted to avoid deleting loops during augment_vertex_set
void VoronoiDiagram::add_split_vertex(HEFace f, Site* s) {
    if (s->isPoint())
        return; // no split-vertices when inserting point-sites
        
    Site* fs = g[f].site;
    
    // don't search for split-vertex on the start or end face
    if (fs->isPoint() && s->isLine()) {
        if ( fs->position() == s->start() || fs->position() == s->end() ) // FIXME: don't compare Points, instead compare vertex-index!
            return;
    }
        
    if ( fs->isPoint() && s->isLine() && s->in_region( fs->position() ) ) {
        // 1) find the correct edge
        Point pt1 = fs->position();
        Point pt2 = pt1-Point( s->a(), s->b() ); 
        
        assert( (pt1-pt2).norm() > 0 ); 
        
        EdgeVector split_edges = find_split_edges(f, pt1, pt2);
        // the sought edge should have src on one side of pt1-pt2
        // and trg on the other side of pt1-pt2
        
        BOOST_FOREACH(HEEdge split_edge, split_edges) {
            if ( (g[split_edge].type == SEPARATOR) || (g[split_edge].type == LINESITE) )
                return; // don't place split points on linesites or separators(?)

            // find a point = src + u*(trg-src)
            // with min_t < u < max_t
            // and minimum distance to the pt1-pt2 line
        #define TOMS748
        
        Point split_pt_pos;
        
        #ifdef TOMS748
            HEVertex split_src = g.source(split_edge);
            HEVertex split_trg = g.target(split_edge);
            if (debug) {
                std::cout << " split src=" << g[split_src].index << "("<< g[split_src].dist() << ")";
                std::cout << " trg=" << g[split_trg].index << "("<< g[split_trg].dist() << ") \n";
                std::cout << "is_right src=" << g[split_src].position.is_right(pt1,pt2) << "  trg="<< g[split_trg].position.is_right(pt1,pt2) << "\n";
            }
            SplitPointError errFunctr(this, split_edge, pt1, pt2); // error functor
            typedef std::pair<double, double> Result;
            boost::uintmax_t max_iter=500;
            boost::math::tools::eps_tolerance<double> tol(64); // bits of tolerance?
            double min_t = std::min( g[split_src].dist() , g[split_trg].dist() );
            double max_t = std::max( g[split_src].dist() , g[split_trg].dist() );
            // require that min_t and max_t bracket the root
            if ( errFunctr(min_t)*errFunctr(max_t) >= 0 )
                return;
                
            Result r1 = boost::math::tools::toms748_solve(errFunctr, min_t, max_t, tol, max_iter);
            split_pt_pos = g[split_edge].point( r1.first ); 
        #endif
        
            // alternative SPLIT-vertex positioning:
            // - create virtual line-site vs: same direction as s(lineSite), but goes through fs(pointSite)
            // - use solver to position SPLIT vertex. The sites are: (vs,fs, fs-adjacent)
        #ifndef TOMS748
            Site* vs = new LineSite(*s);
            vs->set_c( fs->position() ); // modify the line-equation so that the line goes trough fs->position()
            Solution sl = vpos->position( split_edge, vs );
            split_pt_pos = sl.p;
        #endif
        
            HEVertex v = g.add_vertex( VoronoiVertex(split_pt_pos, UNDECIDED, SPLIT, fs->position() ) );
            
        #ifndef TOMS748
            delete vs;
        #endif
        
            //std::cout << "toms748: " << split_pt << "\n";
            //std::cout << "solver:  " << sl.p << "\n";
            if (debug) {
                std::cout << " new split-vertex " << g[v].index << " t=" << r1.first;
                std::cout << " inserted into edge " << g[split_src].index << "-" << g[split_trg].index  << "\n";
            }
            
            assert( vd_checker->check_edge(split_edge) );
            // 3) insert new SPLIT vertex into the edge
            g.add_vertex_in_edge(v, split_edge);
        }
    }
}

// find a SPLIT vertex on the Face f
// return true, and set v, if found.
bool VoronoiDiagram::find_split_vertex(HEFace f, HEVertex& v)  {
    VertexVector verts = g.face_vertices(f);
    BOOST_FOREACH(HEVertex q, verts) {
        if (g[q].type == SPLIT) {
            v = q;
            return true;
        }
    }
    return false;
}

// remove split-vertices on the face f
void VoronoiDiagram::remove_split_vertex(HEFace f) {

    if (debug) {
        std::cout << "remove_split_vertex( " << f << " )\n";
        print_face(f);
    }
    assert( vd_checker->face_ok( f ) );
    
    HEVertex v;
    while ( find_split_vertex(f,v) ) {
        assert(g[v].type == SPLIT); 
        if (debug) std::cout << " removing split-vertex " << g[v].index << "\n";
        
        g.remove_deg2_vertex( v );
        modified_vertices.erase(v);
        
        assert( vd_checker->face_ok( f ) );
    }
    
    assert( vd_checker->face_ok( f ) );
}

// generate new voronoi-vertices on all IN-OUT edges 
// Note: used only by insert_point_site() !!
void VoronoiDiagram::add_vertices( Site* new_site ) {
    if (debug) std::cout << "add_vertices(): \n";
    assert( !v0.empty() );
    EdgeVector q_edges = find_in_out_edges();       // new vertices generated on these IN-OUT edges
    for( unsigned int m=0; m<q_edges.size(); ++m )  {   

        Solution sl = vpos->position( q_edges[m], new_site );
        if ( vpos->dist_error( q_edges[m], sl, new_site) > 1e-9 ) {
            HEVertex src = g.source(q_edges[m]);
            HEVertex trg = g.target(q_edges[m]);
            std::cout << "ERROR while positioning new vertex  on edge\n";
            std::cout << g[ src ].index << "[" << g[ src ].type << "]" << "{" << g[ src ].status << "}" << "(t=" << g[ src ].dist() << ")";
            std::cout <<  " -[" << g[q_edges[m]].type << "]- "; 
            std::cout << g[ trg ].index << "[" << g[ trg ].type << "]" << "{" << g[ trg ].status << "}" << "(t=" << g[ trg ].dist() << ")";
            
            std::cout <<  "     derr =" << vpos->dist_error( q_edges[m], sl, new_site) << "\n";
        }
        HEVertex q = g.add_vertex( VoronoiVertex( sl.p, NEW, NORMAL, new_site->apex_point( sl.p ), sl.k3 ) );
        modified_vertices.insert(q);
        g.add_vertex_in_edge( q, q_edges[m] );
        if (debug) {
            HEVertex src = g.source(q_edges[m]);
            HEVertex trg = g.target(q_edges[m]);
            std::cout << " NEW vertex " << g[q].index << " on edge " << g[src].index << " - " << g[trg].index << "\n";
        }
    }
    if (debug) std::cout << "add_vertices() done.\n";
}

// add a new face corresponding to the new Site
// call add_new_edge() on all the incident_faces that should be split
HEFace VoronoiDiagram::add_face(Site* s) { 
    HEFace newface =  g.add_face(); 
    g[newface].site = s;
    s->face = newface;
    g[newface].status = NONINCIDENT;
    if (s->isPoint() )
        fgrid->add_face( g[newface] ); 
    
    return newface;
}

// by adding a NEW-NEW edge, split the face f into one part which is newface, and the other part is the old f
// for linesegment or arc sites we pass in both the k=+1 face newface and the k=-1 face newface2
void VoronoiDiagram::add_edges(HEFace newface, HEFace f, HEFace newface2) {
    int new_count = num_new_vertices(f);
    if (debug) std::cout << " add_edges() on f=" << f << " with " << new_count << " NEW verts.\n";
    assert( new_count > 0 );
    assert( (new_count % 2) == 0 );
    int new_pairs = new_count / 2; // we add one NEW-NEW edge for each pair found
    VertexVector startverts; // this holds ed.v1 vertices for edges already added
    for (int m=0;m<new_pairs;m++) {
        EdgeData ed = find_edge_data(f, startverts);
        add_edge( ed, newface, newface2);
        startverts.push_back( ed.v1 );
    }
    if (debug) std::cout << " all edges on f=" << f << " added.\n";
}

// newface = the k=+1 positive offset face
// newface2 = the k=-1 negative offset face
void VoronoiDiagram::add_edge(EdgeData ed, HEFace newface, HEFace newface2) {
    HEEdge new_previous = ed.v1_prv;
    HEVertex new_source = ed.v1;         //-OUT-NEW(v1)-IN-...
    HEEdge twin_next = ed.v1_nxt;
    
    HEEdge twin_previous = ed.v2_prv;
    HEVertex new_target = ed.v2;         // -IN-NEW(v2)-OUT-
    HEEdge new_next = ed.v2_nxt;
    
    if (debug)
        std::cout << " add_edge " << g[new_source].index << " - " << g[new_target].index << "\n";
    
    HEFace f = ed.f;
    Site* f_site = g[f].site;
    Site* new_site;
    HEFace new_face;
    if ( g[new_source].k3 == 1 ) { // find out if newface or newface2 should be used
        new_site = g[newface].site;
        new_face = newface; 
    } else {
        new_site = g[newface2].site; 
        new_face = newface2;
    }
        
    // both trg and src should be on same side of new site 
    if (g[new_target].k3 != g[new_source].k3) {
        std::cout << " g[" << g[new_target].index << "].k3=" << g[new_target].k3 << " != ";
        std::cout << "g[" << g[new_source].index << "].k3=" << g[new_source].k3<< "\n";
    }
    assert( g[new_target].k3 == g[new_source].k3 );

    //                                           f
    // now connect:   new_previous -> new_source -> new_target -> new_next
    // and:              twin_next <- new_source <- new_target <- twin_previous 
    //                                           new_face   

    // check for potential apex-split
    bool src_sign=true, trg_sign=true;
    if (f_site->isPoint()  && new_site->isLine() ) { // PL
        Point pt1 = f_site->position();
        Point pt2 = new_site->apex_point(pt1);
        
        src_sign = g[new_source].position.is_right( pt1, pt2 );
        trg_sign = g[new_target].position.is_right( pt1, pt2 );
        
    } else if (f_site->isPoint() && new_site->isPoint() ) { // PP
        src_sign = g[new_source].position.is_right( f_site->position(), new_site->position() );
        trg_sign = g[new_target].position.is_right( f_site->position(), new_site->position() );
    } else if (f_site->isLine() && new_site->isLine() )  { // LL
        //  a line-line bisector, sign should not matter because there is no sqrt()
        
        /*
        std::cout << "add_edge() LL-edge " << g[new_source].index << " - " << g[new_target].index ;
        std::cout << " f_site->k()= " << f_site->k() << " new_site->k()= "<< new_site->k() << "\n";
        std::cout << " f_site <-> src("<< g[new_source].index << ") = " << g[new_source].position.is_right( f_site->start(), f_site->end() ) << " " << g[new_source].position << "\n";
        std::cout << " f_site <-> trg("<< g[new_target].index << ") = " << g[new_target].position.is_right( f_site->start(), f_site->end() ) << " " << g[new_target].position <<  "\n";
        std::cout << " n_site <-> src("<< g[new_source].index << ") = " << g[new_source].position.is_right( new_site->start(), new_site->end() ) << "\n";
        std::cout << " n_site <-> trg("<< g[new_target].index << ") = " << g[new_target].position.is_right( new_site->start(), new_site->end() ) << "\n";
        */
        // this is essentially an in-region test 
        if ( (g[new_source].position != g[new_target].position) && // src and trg are different
              ( g[new_source].position != f_site->start() ) &&  // src/trg is not start or end
              ( g[new_source].position != f_site->end() ) &&
              ( g[new_target].position != f_site->start() ) &&
              ( g[new_target].position != f_site->end() ) &&
              ( (g[new_source].position -f_site->apex_point( g[new_source].position ) ).norm() > 1e-3 ) && // require some distance, 
              ( (g[new_target].position -f_site->apex_point( g[new_target].position ) ).norm() > 1e-3 )  // so that the is_right predicate is accurate
            ) {
                assert( !g[new_source].position.is_right( f_site->start(), f_site->end() ) );
                assert( !g[new_target].position.is_right( f_site->start(), f_site->end() ) );
                assert( !g[new_source].position.is_right( new_site->start(), new_site->end() ) );
                assert( !g[new_target].position.is_right( new_site->start(), new_site->end() ) );
        }

    } else { // unhandled case!
        std::cout << " add_edge() WARNING: no code to deremine src_sign and trg_sign!\n";
        assert(0);
    }
    
    // both src and trg are on the same side of the new site.
    // so no apex-split is required, just add a single edge.
    if ( src_sign == trg_sign ) {  // add a single src-trg edge
        HEEdge e_new, e_twin;
        boost::tie(e_new,e_twin) = g.add_twin_edges( new_source, new_target );
        g[e_new].next = new_next;
        assert( g[new_next].k == g[new_previous].k );
        g[e_new].k = g[new_next].k; // the next edge is on the same face, so has the correct k-value
        g[e_new].face = f; // src-trg edge has f on its left
        g[new_previous].next = e_new;
        g[f].edge = e_new; 
        g[e_new].set_parameters( f_site, new_site, !src_sign ); 
        // the twin edge that bounds the new face
        //HEEdge e_twin = g.add_edge( new_target, new_source );
        g[twin_previous].next = e_twin;
        g[e_twin].next = twin_next;
        g[e_twin].k = g[new_source].k3; 
        g[e_twin].set_parameters( new_site, f_site, src_sign );
        g[e_twin].face = new_face; 
        g[new_face].edge = e_twin;

        assert( vd_checker->check_edge(e_new) && vd_checker->check_edge(e_twin) );
        /*
        std::cout << " added edge " << g[new_target].index << "(" << g[new_target].dist() <<")";
        std::cout << " - " << g[new_source].index << "(" << g[new_source].dist() << ")";
        std::cout << " f=" << g[e_new].face << " k=" << g[e_twin].k;
        std::cout << " twf=" << g[e_twin].face << " twk=" << g[e_new].k;
        std::cout << " type=" << g[e_twin].type <<   " \n";
        */
        //std::cout << " k3 target-source: "<<  g[new_target].k3 << " - " << g[new_source].k3 << "\n";
    } else {
        // need to do apex-split
        //                         f               f  
        //   new_prv -> NEW -- e1 ----> APEX --e2 ---> NEW -> new_nxt
        //   twn_nxt <- NEW <- e1_tw -- APEX <-e2_tw-- NEW <- twn_prv    
        //                       new1/new2         new1/new2
        //   
        HEVertex apex = g.add_vertex( VoronoiVertex(Point(0,0), NEW,APEX) );
        HEEdge e1, e1_tw;
        HEEdge e2, e2_tw;
        boost::tie(e1, e1_tw) = g.add_twin_edges( new_source, apex );
        boost::tie(e2, e2_tw) = g.add_twin_edges( apex, new_target );
        g[e1].set_parameters( f_site, new_site, !src_sign);
        g[e2].set_parameters( f_site, new_site, !trg_sign);
        
        assert( g[new_previous].face == f );
        assert( g[new_next].face == f );
        assert( g[new_next].k == g[new_previous].k );

        // new_previous -> e1 -> e2 -> new_next
        g.set_next_chain( boost::assign::list_of(new_previous)(e1)(e2)(new_next), f, g[new_next].k );
        
    // twin edges
        g[e1_tw].set_parameters(new_site, f_site, src_sign);
        g[e2_tw].set_parameters(new_site, f_site, trg_sign);

        assert( g[twin_previous].k == g[twin_next].k );  
        assert( g[twin_previous].face == g[twin_next].face );        
        // twin_prev -> e2_tw -> e1_tw -> twin_next   on new_face 
        g.set_next_chain( boost::assign::list_of(twin_previous)(e2_tw)(e1_tw)(twin_next) );
        //, new_face,  g[new_source].k3  );
                
        g[e1_tw].k = g[new_source].k3;
        g[e2_tw].k = g[new_source].k3;
        g[new_face].edge = e1_tw;
        g[e1_tw].face = new_face;
        g[e2_tw].face = new_face;
        
        assert( vd_checker->check_edge(e1) && vd_checker->check_edge(e1_tw) );
        assert( vd_checker->check_edge(e2) && vd_checker->check_edge(e2_tw) );
        
    // position the apex
        double min_t = g[e1].minimum_t(f_site,new_site);
        g[apex].position = g[e1].point(min_t);
        g[apex].init_dist(f_site->apex_point(g[apex].position));
        modified_vertices.insert( apex );
    }
}


// we want to insert a SEPARATOR edge (endp, target) , on the give face f.
// find and return the target vertex to which the new SEPARATOR-edge should connect
// also return the adjacent next/prev edges
// flag==true when an OUT-NEW-IN vertex was found
// flag==false when an IN-NEW-OUT vertex was found
boost::tuple<HEEdge,HEVertex,HEEdge,bool> VoronoiDiagram::find_separator_target(HEFace f, HEVertex endp) {
    if (endp==HEVertex()) // no separator
        return boost::make_tuple( HEEdge(), HEVertex(), HEEdge(), false) ;
    
    HEEdge current_edge = g[f].edge; // start on some edge of the face
    HEEdge start_edge = current_edge;
    bool found = false;
    HEVertex v_target;
    HEEdge v_previous, v_next;
    bool flag;
    if (debug) { 
        std::cout << " find_separator_target f=" << f << " endp= " << g[endp].index << "\n";
        print_face(f);
    }
    do {
        HEEdge next_edge = g[current_edge].next;
        HEVertex previous_vertex = g.source( current_edge );
        HEVertex current_vertex  = g.target( current_edge );
        HEVertex next_vertex     = g.target( next_edge );
        bool out_new_in = ( g[previous_vertex].status == OUT && g[current_vertex].status == NEW && g[next_vertex].status == IN );
        bool in_new_out = ( g[previous_vertex].status == IN  && g[current_vertex].status == NEW && g[next_vertex].status == OUT); 
        if ( out_new_in || in_new_out   ) {
            if (debug) {
                std::cout << "OUT/IN-NEW-IN/OUT: " << g[previous_vertex].index << "-" << g[current_vertex].index;
                std::cout << "-" << g[next_vertex].index << "\n";
            }
            if ( (g[endp].k3 == g[current_vertex].k3)  && endp!=current_vertex ) {
                    v_target = current_vertex;
                    v_previous = current_edge;
                    v_next = next_edge;
                    flag = out_new_in ? true : false;
                    found = true;                 
                    if (debug) std::cout << "FOUND!\n";
            }  
        }
        current_edge = g[current_edge].next;   
        //count++;
        //assert(count<10000); // some reasonable max number of edges in face, to avoid infinite loop
    } while (current_edge!=start_edge && !found);
    assert(found);
    return boost::make_tuple(v_previous, v_target, v_next, flag);
}

// on a face which has IN and OUT-vertices, find the sequence
// OUT-OUT-OUT-..-OUT-NEW(v1)-IN-...-IN-NEW(v2)-OUT-OUT
// and return v1/v2 together with their previous and next edges
EdgeData VoronoiDiagram::find_edge_data(HEFace f, VertexVector startverts)  {
    EdgeData ed;
    ed.f = f;
    if (debug) {
        std::cout << "find_edge_data():\n";
        std::cout << " "; print_face(f);
    }
    HEEdge current_edge = g[f].edge; // start on some edge of the face
    HEEdge start_edge = current_edge;
    bool found = false;
    //int count=0;    
    if (debug) std::cout << "    finding OUT-NEW-IN vertex: \n";                         
    do { // find OUT-NEW-IN vertices in this loop
        HEEdge next_edge = g[current_edge].next;
        
        HEVertex previous_vertex = g.source( current_edge);
        HEVertex  current_vertex = g.target( current_edge );
        HEVertex     next_vertex = g.target( next_edge );
        
        if ( (g[current_vertex].status==NEW) && (g[current_vertex].type != SEPPOINT) &&
             (  ((g[previous_vertex].status==OUT || g[previous_vertex].status==UNDECIDED)  && (previous_vertex!=segment_start && previous_vertex!=segment_end))  ||
                ((g[next_vertex].type==ENDPOINT) && (next_vertex==segment_start || next_vertex==segment_end))
             )
           ) {
            bool not_found=true;
            BOOST_FOREACH(HEVertex v, startverts) { // exclude vertices already found
                if (current_vertex==v)
                    not_found=false;
            }
            if (debug) {
                std::cout << "     " << g[current_vertex].index << "N=" << (g[current_vertex].status == NEW) ;
                std::cout << " !SEPP=" << (g[current_vertex].type != SEPPOINT) << "\n";
            }
            if (  not_found ) {
                    ed.v1 = current_vertex;
                    ed.v1_prv = current_edge;
                    ed.v1_nxt = g[current_edge].next;
                    found = true;                 
            }
        }
        current_edge = g[current_edge].next;   
        //count++;
        //assert(count<10000); // some reasonable max number of edges in face, to avoid infinite loop
    } while (current_edge!=start_edge && !found);
    if (!found) {
        std::cout << "ERROR: unable to find OUT-NEW-IN vertex on face:\n";
        print_face(f);
        std::cout << " The excluded vertices are: (size=" << startverts.size()<<")"; print_vertices(startverts);
    }
    assert(found);
    if (debug) std::cout << " OUT-NEW-IN = " << g[ed.v1].index << "\n";

    // now search for v2
    //count=0; 
    start_edge = current_edge;
    found=false;
    if (debug) std::cout << "    finding IN-NEW-OUT vertex: \n";   
    do { // find IN-NEW-OUT vertices in this loop
        HEVertex  current_vertex = g.target( current_edge );
        if ( g[current_vertex].status == NEW && g[current_vertex].type != SEPPOINT ) {
            if (debug) {
                std::cout << "     " << g[current_vertex].index << "N=" << (g[current_vertex].status == NEW) ;
                std::cout << " !SEPP=" << (g[current_vertex].type != SEPPOINT);
                std::cout << " !ed.v1=" << (current_vertex != ed.v1) <<"\n";
            }
            if (  current_vertex != ed.v1) { // -IN-NEW(v2)
                    ed.v2     = current_vertex;
                    ed.v2_prv = current_edge;
                    ed.v2_nxt = g[current_edge].next;
                    found = true;                 
            }
        }
        current_edge = g[current_edge].next;   
        //count++;
        //assert(count<10000); // some reasonable max number of edges in face, to avoid infinite loop
    } while (current_edge!=start_edge && !found);

    assert(found);
    if (debug) std::cout << " IN-NEW-OUT=" << g[ed.v2].index << "\n";

    if (debug) std::cout << "find_edge_data() NEW-NEW vertex pair: " << g[ed.v1].index << " - " << g[ed.v2].index << "\n";
    return ed;
}

// start on g[newface].edge, walk around the face and repair the next-pointers
// this is called on the newly created face after all NEW-NEW edges have been added
void VoronoiDiagram::repair_face( HEFace f ) {
    if (debug) {
        std::cout << "repair_face ( " << f << " ) null1=" << null_face1 << " null2=" << null_face2 << "\n";
    }
    HEEdge current_edge = g[f].edge;
    HEEdge start_edge = current_edge;
    do {
        assert( vd_checker->check_edge(current_edge) );
        HEVertex current_target = g.target( current_edge ); // an edge on the new face
        HEVertex current_source = g.source( current_edge );
        bool found_next_edge= false;
        if (debug) { 
            std::cout << " edge " << g[ g.source(current_edge) ].index << " - ";
            std::cout <<  g[ g.target(current_edge) ].index << "\n";
        }
        BOOST_FOREACH(HEEdge e, g.out_edge_itr(current_target)){
            HEVertex out_target = g.target( e );
            if(debug) {
                std::cout << "     candidate: " << g[ g.source(e) ].index << " - ";
                std::cout << g[ g.target(e) ].index << " f= "<< g[e].face << " \n";
            }
            if ( (out_target != current_source) && 
                 ( (g[out_target].status == NEW)    || 
                   (g[out_target].type == ENDPOINT) || 
                   (g[out_target].type == SEPPOINT) ) ) { // these are the possible vertices we want to go to
                
                
                // special cases where we do a brute-force face-assignment for a null-edge, or a separator
                if ( ((g[e].type == NULLEDGE) &&
                      (g[current_edge].type != NULLEDGE) && // only one null-edge in succession!
                         (
                           // from sep to end
                           ( (g[current_target].type==SEPPOINT) && (g[out_target].type == ENDPOINT) ) ||
                           // or from end -> end to sep
                           ( (g[current_source].type == ENDPOINT) && (g[current_target].type==ENDPOINT)  )
                           ||
                           (out_target == segment_start)
                           ||
                           (out_target == segment_end) 
                         ) &&
                     (g[e].face!=null_face1) && // not along a null-face edge!
                     (g[e].face!=null_face2)
                     ) 
                     ||
                     (g[e].face==zero_point_face) 
                     )
                      {
                         
                    g[e].face = f; // override face-assignment!
                    g[e].k=g[current_edge].k; // override k-assignment!
                }
                    
                // the next vertex should not where we came from
                // and it should be on the same face.
                if (  (g[e].face == f) ) {  
                    g[current_edge].next = e; // this is the edge we want to take
                    found_next_edge = true;
                    if(debug) {
                        std::cout << "         next: " << g[ g.source(e) ].index << " - ";
                        std::cout << g[ g.target(e) ].index << "\n";
                    }
                    assert( g[current_edge].k == g[e].k );
                    assert( vd_checker->current_face_equals_next_face( current_edge ) );
                }
            } 
        }
        if (!found_next_edge)
            std::cout << " repair_face( " << f << " ) error. could not find next-edge!\n";
        assert(found_next_edge); // must find a next-edge!
         
        current_edge = g[current_edge].next; // jump to the next edge
    } while (current_edge != start_edge);
    
}

// remove the IN vertices stored in v0 (and associated IN-NEW edges)
void VoronoiDiagram::remove_vertex_set() {
    BOOST_FOREACH( HEVertex& v, v0 ) {      // it should now be safe to delete all IN vertices
        assert( g[v].status == IN );
        g.delete_vertex(v); // this also removes edges connecting to v
        modified_vertices.erase(v);
    }
}
    
// at the end after an incremental insertion of a new site,
// reset status of modified_vertices to UNDECIDED and incident_faces to NONINCIDENT,
// so that we are ready for the next insertion.
void VoronoiDiagram::reset_status() {
    BOOST_FOREACH( HEVertex v, modified_vertices ) {
        g[v].reset();
    }
    modified_vertices.clear();
    BOOST_FOREACH(HEFace& f, incident_faces ) { 
        g[f].status = NONINCIDENT; 
    }
    incident_faces.clear();
    v0.clear();
}

// given the set v0 of "IN" vertices, find and return the adjacent IN-OUT edges 
// later NEW-vertices are inserted into each of the found IN-OUT edges
EdgeVector VoronoiDiagram::find_in_out_edges() { 
    assert( !v0.empty() );
    EdgeVector output; // new vertices generated on these edges
    BOOST_FOREACH( HEVertex& v, v0 ) {                                   
        assert( g[v].status == IN ); // all verts in v0 are IN
        BOOST_FOREACH(HEEdge e, g.out_edge_itr(v)){
            if ( g[ g.target( e ) ].status == OUT ) 
                output.push_back(e); // this is an IN-OUT edge
        }
    }
    assert( !output.empty() );
    return output;
}

// number of IN vertices adjacent to given vertex v
// predicate C4 i.e. "adjacent in-count" from Sugihara&Iri 1992 "one million" paper
bool VoronoiDiagram::predicate_c4(HEVertex v) {
    int in_count=0;
    BOOST_FOREACH(HEEdge e, g.out_edge_itr(v)){
        HEVertex w = g.target( e );
        if ( g[w].status == IN ) {
            in_count++;
            if (in_count >= 2)
                return true;
        }
    }
    return false;
}

// do any of the three faces that are adjacent to the given IN-vertex v have an IN-vertex ?
// predicate C5 i.e. "connectedness"  from Sugihara&Iri 1992 "one million" paper
bool VoronoiDiagram::predicate_c5(HEVertex v) {
    if (g[v].type == APEX || g[v].type == SPLIT ) { return true; } // ?
    FaceVector adjacent_incident_faces;

    BOOST_FOREACH(HEEdge e, g.out_edge_itr(v)){
        HEFace f = g[e].face;
        if ( g[f].status == INCIDENT )
            adjacent_incident_faces.push_back( f );
    }

    assert( !adjacent_incident_faces.empty() );
    
    bool all_found = true;
    BOOST_FOREACH( HEFace f, adjacent_incident_faces ) { // check each adjacent face f for an IN-vertex
        bool face_ok=false;
        HEEdge current = g[f].edge;
        HEEdge start = current;
        do {
            HEVertex w = g.target(current);
            if ( w != v && g[w].status == IN && g.has_edge(w,v) )  // v should be adjacent to an IN vertex on the face
                face_ok = true;
            else if ( w!=v && ( g[w].type == ENDPOINT || g[w].type == APEX  || g[w].type == SPLIT) ) // if we are next to an ENDPOINT, then ok(?)
                face_ok=true;
            current = g[current].next;
        } while(current!=start);  
        
        if (!face_ok)
            all_found=false;
    }
    return all_found; // if this returns false, we mark a vertex OUT, on topology grounds.
}

// count the total number of SPLIT vertices
int VoronoiDiagram::num_split_vertices()  { 
    int count = 0;
    BOOST_FOREACH( HEVertex v, g.vertices() ) {
        if (g[v].type == SPLIT)
            count++;
    }
    return count; 
}

// count number of NEW vertices on the given face
int VoronoiDiagram::num_new_vertices(HEFace f) {
    HEEdge current = g[f].edge;
    HEEdge start = current;
    int count=0;
    //int num_e=0;
    do {
        HEVertex v = g.target(current);
        if ( (g[v].status == NEW) && (g[v].type != SEPPOINT) )
            count++;
        //num_e++;
        //assert( num_e <3000);
        current = g[current].next;
    } while(current!=start);  
    return count;
}

bool VoronoiDiagram::check() {
    if( vd_checker->is_valid() ) {
        if (debug) std::cout << "diagram check OK.\n";
        return true;
    } else {
        if (debug) std::cout << "diagram check ERROR.\n";
        return false;
    }
}

void VoronoiDiagram::print_faces() {
    for( HEFace f=0;f<g.num_faces();f++) {
        print_face(f);
    }
}

void VoronoiDiagram::print_face(HEFace f) {
    std::cout << " Face " << f << ": ";
    HEEdge current = g[f].edge;
    HEEdge start=current;
    int num_e=0;
    do {
        HEVertex v = g.source(current);
        std::cout << g[v].index  << "(" << g[v].status  << ")-f"<< g[current].face << "-";
        num_e++;
        assert(num_e<30);
        current = g[current].next;
    } while ( current!=start );
    std::cout << "\n";
}

void VoronoiDiagram::print_edges(EdgeVector& q) {
    BOOST_FOREACH( HEEdge e, q ) {
        HEVertex src = g.source(e);
        HEVertex trg = g.target(e);
        std::cout << g[src].index << "-" << g[trg].index << "\n";
    }
}

void VoronoiDiagram::print_edge(HEEdge e) {
        HEVertex src = g.source(e);
        HEVertex trg = g.target(e);
        std::cout << g[src].index << "-f" << g[e].face << "-" << g[trg].index << "\n";
}

void VoronoiDiagram::print_vertices(VertexVector& q) {
    BOOST_FOREACH( HEVertex v, q) {
        std::cout << g[v].index << "["<< g[v].type << "]" << " ";
    }
    std::cout << std::endl;
}

std::string VoronoiDiagram::print() const {
    std::ostringstream o;
    o << "VoronoiDiagram \n";
    o << " num_vertices    = "<< g.num_vertices() << "\n";
    o << " num_edges       = "<< g.num_edges() <<"\n";
    o << " num_point_sites = "<< num_point_sites() <<"\n";
    o << " num_line_sites  = "<< num_line_sites() <<"\n";
    return o.str();
}

} // end namespace
// end file voronoidiagram.cpp
