/*
 *  Copyright (c) 2012-2014, Bruno Levy
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  * Neither the name of the ALICE Project-Team nor the names of its
 *  contributors may be used to endorse or promote products derived from this
 *  software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  If you modify this software, you should include a notice giving the
 *  name of the person performing the modification, the date of modification,
 *  and the reason for such modification.
 *
 *  Contact: Bruno Levy
 *
 *     Bruno.Levy@inria.fr
 *     http://www.loria.fr/~levy
 *
 *     ALICE Project
 *     LORIA, INRIA Lorraine,
 *     Campus Scientifique, BP 239
 *     54506 VANDOEUVRE LES NANCY CEDEX
 *     FRANCE
 *
 */

// A mesh file converter using Geogram.

#include <geogram/basic/command_line.h>
#include <geogram/basic/command_line_args.h>
#include <geogram/basic/logger.h>
#include <geogram/basic/progress.h>
#include <geogram/basic/stopwatch.h>
#include <geogram/mesh/mesh.h>
#include <geogram/mesh/mesh_geometry.h>
#include <geogram/mesh/mesh_io.h>
#include <geogram/mesh/mesh_AABB.h>
#include <algorithm>
#include <array>
#include <iterator>

////////////////////////////////////////////////////////////////////////////////

bool endswith (std::string const &str, std::string const &ending) {
	if (str.length() >= ending.length()) {
		return (0 == str.compare(str.length() - ending.length(), ending.length(), ending));
	} else {
		return false;
	}
}

template <typename Scalar, size_t Rows>
inline std::ostream& operator<<(std::ostream &out, std::array<Scalar, Rows> v) {
	out << "{";
	if (!v.empty()) {
		std::copy(v.begin(), v.end() - 1, std::ostream_iterator<Scalar>(out, "; "));
		out << v.back();
	}
	out << "}";
	return out;
}

////////////////////////////////////////////////////////////////////////////////

namespace Layout {

	GEO::vec3i index3_from_index(int idx, GEO::vec3i size) {
		return GEO::vec3i(
			idx % size[0],
			(idx / size[0]) % size[1],
			(idx / size[0]) / size[1]
		);
	}

	int index_from_index3(GEO::vec3i vx, GEO::vec3i size) {
		return (vx[2] * size[1] + vx[1]) * size[0] + vx[0];
	}

}

namespace GEO {

	bool filename_has_supported_extension(const std::string &filename) {
		std::vector<std::string> extensions;
		GEO::MeshIOHandlerFactory::list_creators(extensions);
		for (auto &ext : extensions) {
			if (endswith(filename, ext)) {
				return true;
			}
		}
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////

template<typename T>
class VoxelGrid {
private:
	// Member data
	std::vector<T> m_data;
	GEO::vec3      m_origin;
	double         m_spacing; // voxel size (in mm)
	GEO::vec3i     m_grid_size;

public:
	// Interface
	VoxelGrid(GEO::vec3 origin, GEO::vec3 extent, double voxel_size, int padding);

	GEO::vec3i grid_size() const { return m_grid_size; }
	int num_voxels() const { return m_grid_size[0] * m_grid_size[1]  * m_grid_size[2]; }

	GEO::vec3 origin() const { return m_origin; }
	double spacing() const { return m_spacing; }

	GEO::vec3i index3_from_index(int idx) const { return Layout::index3_from_index(idx, m_grid_size); }
	int index_from_index3(GEO::vec3i vx) const { return Layout::index_from_index3(vx, m_grid_size); }

	GEO::vec3 voxel_center(int x, int y, int z) const;

	const T at(int idx) const { return m_data[idx]; }
	T & at(int idx) { return m_data[idx]; }
	const T * rawbuf() const { return m_data.data(); }
	T * raw_layer(int z) { return m_data.data() + z * m_grid_size[1] * m_grid_size[0]; }
};

////////////////////////////////////////////////////////////////////////////////

template<typename T>
VoxelGrid<T>::VoxelGrid(GEO::vec3 origin, GEO::vec3 extent, double spacing, int padding)
	: m_origin(origin)
	, m_spacing(spacing)
{
	m_origin -= padding * spacing * GEO::vec3(1, 1, 1);
	m_grid_size[0] = (int) std::ceil(extent[0] / spacing) + 2 * padding;
	m_grid_size[1] = (int) std::ceil(extent[1] / spacing) + 2 * padding;
	m_grid_size[2] = (int) std::ceil(extent[2] / spacing) + 2 * padding;
	GEO::Logger::out("Voxels") << "Grid size: "
		<< m_grid_size[0] << " x " << m_grid_size[1] << " x " << m_grid_size[2] << std::endl;
	m_data.assign(m_grid_size[0] * m_grid_size[1] * m_grid_size[2], T(0));
}

template<typename T>
GEO::vec3 VoxelGrid<T>::voxel_center(int x, int y, int z) const {
	GEO::vec3 pos;
	pos[0] = (x + 0.5) * m_spacing;
	pos[1] = (y + 0.5) * m_spacing;
	pos[2] = (z + 0.5) * m_spacing;
	return pos + m_origin;
}

////////////////////////////////////////////////////////////////////////////////
// NOTE: Function `point_in_triangle_2d` comes from SDFGen by Christopher Batty.
// https://github.com/christopherbatty/SDFGen/blob/master/makelevelset3.cpp
////////////////////////////////////////////////////////////////////////////////

// calculate twice signed area of triangle (0,0)-(x1,y1)-(x2,y2)
// return an SOS-determined sign (-1, +1, or 0 only if it's a truly degenerate triangle)
int orientation(
	double x1, double y1, double x2, double y2, double &twice_signed_area)
{
	twice_signed_area=y1*x2-x1*y2;
	if(twice_signed_area>0) return 1;
	else if(twice_signed_area<0) return -1;
	else if(y2>y1) return 1;
	else if(y2<y1) return -1;
	else if(x1>x2) return 1;
	else if(x1<x2) return -1;
	else return 0; // only true when x1==x2 and y1==y2
}

// -----------------------------------------------------------------------------

// robust test of (x0,y0) in the triangle (x1,y1)-(x2,y2)-(x3,y3)
// if true is returned, the barycentric coordinates are set in a,b,c.
bool point_in_triangle_2d(
	double x0, double y0, double x1, double y1,
	double x2, double y2, double x3, double y3,
	double &a, double &b, double &c)
{
	x1-=x0; x2-=x0; x3-=x0;
	y1-=y0; y2-=y0; y3-=y0;
	int signa=orientation(x2, y2, x3, y3, a);
	if(signa==0) return false;
	int signb=orientation(x3, y3, x1, y1, b);
	if(signb!=signa) return false;
	int signc=orientation(x1, y1, x2, y2, c);
	if(signc!=signa) return false;
	double sum=a+b+c;
	geo_assert(sum!=0); // if the SOS signs match and are nonzero, there's no way all of a, b, and c are zero.
	a/=sum;
	b/=sum;
	c/=sum;
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool intersect_ray_z(const GEO::Mesh &M, GEO::index_t f, const GEO::vec3 &q, double &z) {
	using namespace GEO;

	index_t c = M.facets.corners_begin(f);
	const vec3& p1 = Geom::mesh_vertex(M, M.facet_corners.vertex(c++));
	const vec3& p2 = Geom::mesh_vertex(M, M.facet_corners.vertex(c++));
	const vec3& p3 = Geom::mesh_vertex(M, M.facet_corners.vertex(c));

	double u, v, w;
	if (point_in_triangle_2d(
		q[0], q[1], p1[0], p1[1], p2[0], p2[1], p3[0], p3[1], u, v, w))
	{
		z = u*p1[2] + v*p2[2] + w*p3[2];
		return true;
	}

	return false;
}

// -----------------------------------------------------------------------------

template<typename T>
void compute_sign(const GEO::Mesh &M,
	const GEO::MeshFacetsAABB &aabb_tree, VoxelGrid<T> &voxels)
{
	const GEO::vec3i size = voxels.grid_size();

	try {
		GEO::ProgressTask task("Ray marching", 100);

		GEO::vec3 min_corner, max_corner;
		GEO::get_bbox(M, &min_corner[0], &max_corner[0]);

		const GEO::vec3 origin = voxels.origin();
		const double spacing = voxels.spacing();

		GEO::parallel_for([&](int y) {
			if (GEO::Thread::current()->id() == 0) {
				task.progress((int) (100.0 * y / size[1] * GEO::Process::number_of_cores()));
			}
			for (int x = 0; x < size[0]; ++x) {
				GEO::vec3 center = voxels.voxel_center(x, y, 0);

				GEO::Box box;
				box.xyz_min[0] = box.xyz_max[0] = center[0];
				box.xyz_min[1] = box.xyz_max[1] = center[1];
				box.xyz_min[2] = min_corner[2] - spacing;
				box.xyz_max[2] = max_corner[2] + spacing;

				std::vector<double> inter;
				auto action = [&M, &inter, &center] (GEO::index_t f) {
					double z;
					if (intersect_ray_z(M, f, center, z)) {
						inter.push_back(z);
					}
				};
				aabb_tree.compute_bbox_facet_bbox_intersections(box, action);
				std::sort(inter.begin(), inter.end());

				for (size_t k = 1; k < inter.size(); k += 2) {
					int z1 = int(std::round((inter[k-1] - origin[2])/spacing));
					int z2 = int(std::round((inter[k] - origin[2])/spacing));
					z1 = std::max(0, std::min(z1, size[2]));
					z2 = std::max(0, std::min(z2, size[2]));
					for (int z = z1; z < z2; ++z) {
						geo_assert(z >= 0 && z < size[2]);
						const int idx = voxels.index_from_index3(GEO::vec3i(x, y, z));
						voxels.at(idx) = T(1) - voxels.at(idx);
					}
				}
			}
		}, 0, size[1]);
	} catch(const GEO::TaskCanceled&) {
		// Do early cleanup
	}
}

// -----------------------------------------------------------------------------

typedef unsigned char num_t;

void paraview_dump(std::string &basename, const VoxelGrid<num_t> &voxels) {
	GEO::vec3i size = voxels.grid_size();

	std::ofstream metafile(basename + ".mhd");
	metafile << "ObjectType = Image\nNDims = 3\n"
		<< "DimSize = " << size[0] << " " << size[1] << " " << size[2] << "\n"
		<< "ElementType = MET_CHAR\nElementDataFile = " + basename + ".raw\n";
	metafile.close();

	std::ofstream rawfile(basename + ".raw", std::ios::binary);
	rawfile.write(reinterpret_cast<const char*>(voxels.rawbuf ()), voxels.num_voxels() * sizeof(num_t));
	rawfile.close();
}

////////////////////////////////////////////////////////////////////////////////

void triangle_mesh_dump(std::string &filename, const VoxelGrid<num_t> &voxels) {
	using GEO::vec3i;

	vec3i cell_size = voxels.grid_size();
	vec3i node_size = cell_size + vec3i(1, 1, 1);
	int num_cells = voxels.num_voxels();
	int num_nodes = node_size[0] * node_size[1] * node_size[2];

	// Create triangle list from voxel grid
	GEO::vector<GEO::index_t> triangles;
	for (int idx = 0; idx < num_cells; ++idx) {
		vec3i pos = voxels.index3_from_index(idx);

		// Skip empty voxels
		if (voxels.at(idx) == num_t(0)) { continue; }

		// Define corner index
		std::array<GEO::index_t, 8> corners;
		corners[0] = Layout::index_from_index3(pos + vec3i(0, 0, 0), node_size);
		corners[1] = Layout::index_from_index3(pos + vec3i(1, 0, 0), node_size);
		corners[2] = Layout::index_from_index3(pos + vec3i(1, 1, 0), node_size);
		corners[3] = Layout::index_from_index3(pos + vec3i(0, 1, 0), node_size);
		corners[4] = Layout::index_from_index3(pos + vec3i(0, 0, 1), node_size);
		corners[5] = Layout::index_from_index3(pos + vec3i(1, 0, 1), node_size);
		corners[6] = Layout::index_from_index3(pos + vec3i(1, 1, 1), node_size);
		corners[7] = Layout::index_from_index3(pos + vec3i(0, 1, 1), node_size);

		// Subroutine to emit a facet quad
		auto check_facet = [&](int axis, int delta, int v1, int v2, int v3, int v4) {
			// Compute neigh voxel position
			vec3i neigh = pos;
			neigh[axis] += delta;

			// Check whether neigh voxel is empty
			bool neigh_is_empty = false;
			if (neigh[axis] < 0 || neigh[axis] >= cell_size[axis]) {
				neigh_is_empty = true;
			} else {
				int neigh_idx = voxels.index_from_index3(neigh);
				neigh_is_empty = (voxels.at(neigh_idx) == num_t(0));
			}

			// If neigh voxel is empty, emit triangle strips
			if (neigh_is_empty) {
				triangles.insert(triangles.end(), { corners[v1], corners[v2], corners[v3] });
				triangles.insert(triangles.end(), { corners[v3], corners[v2], corners[v4] });
			}
		};

		// Check adjacency and emit facets
		check_facet(0, -1, 0, 4, 3, 7); // left facet
		check_facet(0,  1, 2, 6, 1, 5); // right facet
		check_facet(1, -1, 1, 5, 0, 4); // front facet
		check_facet(1,  1, 3, 7, 2, 6); // back facet
		check_facet(2, -1, 0, 1, 3, 2); // lower facet
		check_facet(2,  1, 4, 5, 7, 6); // upper facet
	}

	// Assign vertex id (and remap triangle list)
	int num_vertices = 0;
	std::vector<int> node_id(num_nodes, -1);
	for (GEO::index_t &c : triangles) {
		if (node_id[c] == -1) {
			node_id[c] = num_vertices++;
		}
		c = node_id[c];
	}

	// Create Geogram mesh
	GEO::Mesh M;
	M.vertices.create_vertices(num_vertices);
	for (int v = 0; v < num_nodes; ++v) {
		if (node_id[v] != -1) {
			vec3i pos = Layout::index3_from_index(v, node_size);
			M.vertices.point(node_id[v]) = GEO::vec3(pos);
		}
	}
	M.facets.assign_triangle_mesh(triangles, true);

	// Connect facets
	M.facets.connect();

	// Rescale to unit box, and set min corner to 0
	// TODO: Add option to normalize, or output original real-world positions
	GEO::vec3 min_corner, max_corner;
	GEO::get_bbox(M, &min_corner[0], &max_corner[0]);
	GEO::vec3 extent = max_corner - min_corner;
	double scaling = std::max(extent[0], std::max(extent[1], extent[2]));
	for (int v = 0; v < M.vertices.nb(); ++v) {
		M.vertices.point(v) = (M.vertices.point(v) - min_corner) / scaling;
	}

	// Save mesh
	GEO::mesh_save(M, filename);
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
	// Initialize the Geogram library
	GEO::initialize();

	// Import standard command line arguments, and custom ones
	GEO::CmdLine::import_arg_group("standard");
	GEO::CmdLine::declare_arg("padding", 0, "Number of padded grid cells");
	GEO::CmdLine::declare_arg("resolution", 1.0, "Size of a voxel (in mm)");
	GEO::CmdLine::declare_arg("numvoxels", -1, "Number of voxels along the longest axis");

	// Parse command line options and filenames
	std::vector<std::string> filenames;
	if(!GEO::CmdLine::parse(argc, argv, filenames, "in_mesh_file <out_voxel_file>")) {
		return 1;
	}

	int padding = std::max(0, GEO::CmdLine::get_arg_int("padding"));
	double voxel_size = GEO::CmdLine::get_arg_double("resolution");
	int num_voxels = GEO::CmdLine::get_arg_int("numvoxels");

	// Default output filename is "output" if unspecified
	if(filenames.size() == 1) {
		filenames.push_back("output");
	}

	// Display input and output filenames
	GEO::Logger::div("Command line");
	GEO::Logger::out("VoxMesh") << "Input file: " << filenames[0] << std::endl;
	GEO::Logger::out("VoxMesh") << "Output file: " << filenames[1] << std::endl;

	// Declare a mesh
	GEO::Mesh M;

	// Load the mesh and display timings
	GEO::Logger::div("Loading");
	{
		GEO::Stopwatch W("Load");
		if(!GEO::mesh_load(filenames[0], M)) {
			return 1;
		}
		geo_assert(M.vertices.dimension() == 3);
	}

	// Initialize voxel grid and AABB tree
	GEO::vec3 min_corner, max_corner;
	GEO::get_bbox(M, &min_corner[0], &max_corner[0]);
	GEO::vec3 extent = max_corner - min_corner;
	if (num_voxels > 0) {
		// Force number of voxels along longest axis
		double max_extent = std::max(extent[0], std::max(extent[1], extent[2]));
		voxel_size = max_extent / num_voxels;
	}
	VoxelGrid<num_t> voxels(min_corner, extent, voxel_size, padding);
	GEO::MeshFacetsAABB aabb_tree(M);

	// Compute inside/outside info
	GEO::Logger::div("Voxelizing");
	compute_sign(M, aabb_tree, voxels);

	// Save voxel grid and display timings
	GEO::Logger::div("Saving");
	{
		GEO::Stopwatch W("Save");
		if (GEO::filename_has_supported_extension(filenames[1])) {
			triangle_mesh_dump(filenames[1], voxels);
		} else {
			paraview_dump(filenames[1], voxels);
		}
	}

	return 0;
}
