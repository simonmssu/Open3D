// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2020 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#include "../TensorFlowHelper.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/shape_inference.h"
#include "tensorflow/core/lib/core/errors.h"

using namespace tensorflow;

REGISTER_OP("Open3DFixedRadiusSearch")
        .Attr("T: {float, double}")
        .Attr("metric: {'L1', 'L2', 'Linf'} = 'L2'")
        .Attr("ignore_query_point: bool = false")
        .Attr("return_distances: bool = false")
        .Input("points: T")
        .Input("queries: T")
        .Input("radius: T")
        .Input("hash_table_index: uint32")
        .Input("hash_table_row_splits: uint32")
        .Output("neighbors_index: int32")
        .Output("neighbors_row_splits: int64")
        .Output("neighbors_distance: T")
        .SetShapeFn([](::tensorflow::shape_inference::InferenceContext* c) {
            using namespace ::tensorflow::shape_inference;
            using namespace open3d::ml::shape_checking;
            ShapeHandle points_shape, queries_shape, radius_shape,
                    hash_table_index_shape, hash_table_row_splits_shape,
                    neighbors_index_shape, neighbors_row_splits_shape,
                    neighbors_distance_shape;

            TF_RETURN_IF_ERROR(c->WithRank(c->input(0), 2, &points_shape));
            TF_RETURN_IF_ERROR(c->WithRank(c->input(1), 2, &queries_shape));
            TF_RETURN_IF_ERROR(c->WithRank(c->input(2), 0, &radius_shape));
            TF_RETURN_IF_ERROR(
                    c->WithRank(c->input(3), 1, &hash_table_index_shape));
            TF_RETURN_IF_ERROR(
                    c->WithRank(c->input(4), 1, &hash_table_row_splits_shape));

            Dim num_points("num_points");
            Dim num_queries("num_queries");
            CHECK_SHAPE_HANDLE(c, points_shape, num_points, 3);
            CHECK_SHAPE_HANDLE(c, hash_table_index_shape, num_points);
            CHECK_SHAPE_HANDLE(c, queries_shape, num_queries, 3);

            // we cannot infer the number of neighbors
            neighbors_index_shape = c->MakeShape({c->UnknownDim()});
            c->set_output(0, neighbors_index_shape);

            neighbors_row_splits_shape = MakeShapeHandle(c, num_queries + 1);
            c->set_output(1, neighbors_row_splits_shape);

            bool return_distances;
            TF_RETURN_IF_ERROR(
                    c->GetAttr("return_distances", &return_distances));
            if (return_distances)
                neighbors_distance_shape = c->MakeShape({c->UnknownDim()});
            else
                neighbors_distance_shape = c->MakeShape({0});
            c->set_output(2, neighbors_distance_shape);

            return Status::OK();
        })
        .Doc(R"doc(
Computes the indices of all neighbors within a radius.

This op computes the neighborhood for each query point and returns the indices 
of the neighbors.

metric:
  Either L1, L2 or Linf. Default is L2

ignore_query_point:
  If true the points that coincide with the center of the search window will be
  ignored. This excludes the query point if 'queries' and 'points' are the same 
  point cloud.

return_distances:
  If True the distances for each neighbor will be returned in the tensor 
  'neighbors_distance'.
  If False a zero length Tensor will be returned for 'neighbors_distance'.

points: 
  The 3D positions of the input points.

queries: 
  The 3D positions of the query points.

radius:
  A scalar with the neighborhood radius

hash_table_index: Stores the values of the hash table, which are the indices of
  the points. The start and end of each cell is defined by hash_table_row_splits.

hash_table_row_splits: Defines the start and end of each hash table cell.

neighbors_index:
  The compact list of indices of the neighbors. The corresponding query point 
  can be inferred from the 'neighbor_count_row_splits' vector.

neighbors_row_splits:
  The exclusive prefix sum of the neighbor count for the query points including
  the total neighbor count as the last element. The size of this array is the 
  number of queries + 1.

neighbors_distance:
  Stores the distance to each neighbor if 'return_distances' is True.
  Note that the distances are squared if metric is L2.
  This is a zero length Tensor if 'return_distances' is False. 

)doc");
