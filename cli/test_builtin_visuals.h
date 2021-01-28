#ifndef VKY_TEST_BUILTIN_VISUALS_HEADER
#define VKY_TEST_BUILTIN_VISUALS_HEADER

#include "../include/visky/visuals.h"
#include "utils.h"



/*************************************************************************************************/
/*  Visual example                                                                               */
/*************************************************************************************************/



/*************************************************************************************************/
/*  Visuals tests                                                                                */
/*************************************************************************************************/

// Basic visuals.
int test_visuals_point(TestContext* context);
int test_visuals_line(TestContext* context);
int test_visuals_line_strip(TestContext* context);
int test_visuals_triangle(TestContext* context);
int test_visuals_triangle_strip(TestContext* context);
int test_visuals_triangle_fan(TestContext* context);

// 2D visuals.
int test_visuals_marker(TestContext* context);
int test_visuals_axes_2D_1(TestContext* context);
int test_visuals_axes_2D_update(TestContext* context);

// 3D visuals.
int test_visuals_mesh(TestContext* context);
int test_visuals_volume_slice(TestContext* context);



#endif
