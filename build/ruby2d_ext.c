#define MRUBY 1

// canvas.c

#include "ruby2d.h"

typedef SDL_FPoint vector_t;

static float vector_magnitude(const vector_t *vec)
{
  return sqrtf((vec->x * vec->x) + (vec->y * vec->y));
}

static vector_t *vector_normalize(vector_t *vec)
{
  float mag = vector_magnitude(vec);
  if (mag == 0)
    mag = INFINITY;
  vec->x /= mag;
  vec->y /= mag;
  return vec;
}

static vector_t *vector_perpendicular(vector_t *vec)
{
  float tmp_x = vec->x;
  vec->x = vec->y;
  vec->y = -tmp_x;
  return vec;
}

static vector_t *vector_times_scalar(vector_t *vec, float value)
{
  vec->x *= value;
  vec->y *= value;
  return vec;
}

static vector_t *vector_minus_vector(vector_t *vec, const vector_t *other)
{
  vec->x -= other->x;
  vec->y -= other->y;
  return vec;
}

/* Unused
static vector_t *vector_minus_xy(vector_t *vec, float other_x, float other_y)
{
  vec->x -= other_x;
  vec->y -= other_y;
  return vec;
}
*/

static vector_t *vector_plus_vector(vector_t *vec, const vector_t *other)
{
  vec->x += other->x;
  vec->y += other->y;
  return vec;
}

/* Unused
static vector_t *vector_plus_xy(vector_t *vec, float other_x, float other_y)
{
  vec->x += other_x;
  vec->y += other_y;
  return vec;
}
*/

typedef enum { CONCAVE = -1, INVALID, CONVEX } poly_type_e;

//
// Go through each corner and identify and count the number of concave
// ones, also noting down the last concave point.
//
// TODO: collect all the concave points for a later ear-clipper.
// @param [int *] pconcave_ix Pointer to array of slots for concave point
// indices
// @param [int] max_pconcave Max number of clots for concave point indices
//
// @return number of concave points; note this may be > +max_pconcave+ so caller
//                needs to check result carefully.
//
static int count_concave_corners(const SDL_FPoint *points, int num_points,
                                 int *pconcave_ix, const int max_pconcave)
{
  // triangles are always convex
  if (num_points <= 3)
    return (0);

  int nconcave = 0;

  // pick first last and first points
  int x1 = points[0].x;
  int y1 = points[0].y;
  int x2 = points[1].x;
  int y2 = points[1].y;
  // start with second point and proceed point by point inspecting
  // each corner
  float z, zprev = 0;
  for (int i = 1; i < num_points; i++, zprev = z) {
    int j = (i + 1) % num_points;
    int x3 = points[j].x;
    int y3 = points[j].y;
    // cross product
    z = ((x1 - x2) * (y2 - y3)) - ((y1 - y2) * (x2 - x3));
    if (z < 0) {
      if (nconcave < max_pconcave)
        *pconcave_ix++ = i;
      nconcave++;
    }
    // shift points left so we
    x1 = x2;
    y1 = y2;
    x2 = x3;
    y2 = y3;
  }
  return nconcave;
}

//
// If a polygon is entirely conves, we triangulate trivially
// by using the first point as the anchor to fan out the triangles
static void fill_convex_polygon(SDL_Renderer *render, const SDL_FPoint *points,
                                int num_points, const SDL_Color *colors,
                                int num_colors)
{
  // Setup the vertices from incoming arguments
  SDL_Vertex verts[3];

  // use first vertex as anchor and draw a triangle with
  // subsequent pair of points.
  verts[0].position = points[0];
  verts[0].color = colors[0];
  for (int i = 2; i < num_points; i++) {
    verts[1].position = points[i - 1];
    verts[1].color = colors[(i - 1) % num_colors];
    verts[2].position = points[i];
    verts[2].color = colors[i % num_colors];
    SDL_RenderGeometry(render, NULL, verts, 3, NULL, 0);
  }
}

//
// If a polygon has one concave corner, we can still do trivial
// triangulation as we do for convex polygons using the
// concave corner as the anchor point for the fan-out.
static void fill_concave1pt_polygon(SDL_Renderer *render,
                                    const SDL_FPoint *points, int num_points,
                                    int anchor_ix, const SDL_Color *colors,
                                    int num_colors)
{
  // Setup the vertices from incoming arguments
  SDL_Vertex verts[3];

  verts[0].position = points[anchor_ix];
  verts[0].color = colors[anchor_ix % num_colors];
  anchor_ix = (anchor_ix + 1) % num_points;
  for (int i = 2; i < num_points; i++) {
    verts[1].position = points[anchor_ix];
    verts[1].color = colors[anchor_ix % num_colors];
    anchor_ix = (anchor_ix + 1) % num_points;
    verts[2].position = points[anchor_ix];
    verts[2].color = colors[anchor_ix % num_colors];
    SDL_RenderGeometry(render, NULL, verts, 3, NULL, 0);
  }
}

#define MAX_CONCAVE_POINT_INDICES 32

/*
 * Fill a polygon with a single colour.
 * @param points Array of points
 * @param num_points Number of points
 * @param colors Array of colours
 * @param num_colors Number of colors, must be at least 1, and may be less than
 *                      +num_points+ in which case the colours will be repeated
 *                      via modulo.
 * @note Currently supports only: convex polygons or simple polygons with one
 * concave corner. For now.
 */
void R2D_Canvas_FillPolygon(SDL_Renderer *render, SDL_FPoint *points,
                            int num_points, SDL_Color *colors, int num_colors)
{
  if (num_points < 3)
    return;

  // poly_type_e type = polygon_type(points, num_points);
  int concave_point_indices[MAX_CONCAVE_POINT_INDICES];

  int nconcave =
      num_points == 3
          ? 0 // triangles are always convex
          : count_concave_corners(points, num_points, concave_point_indices,
                                  MAX_CONCAVE_POINT_INDICES);

  if (nconcave == 0) {
    // convex
    fill_convex_polygon(render, points, num_points, colors, num_colors);
  }
  else if (nconcave == 1) {
    // use concave point as the origin if only one concave vertex
    fill_concave1pt_polygon(render, points, num_points,
                            concave_point_indices[0], colors, num_colors);
  }
  // else
  //   printf("TODO: Non-convex polygon with %d concave corners\n", nconcave);
}

/*
 * Draw a thick line on a canvas using a pre-converted RGBA colour value.
 * @param [int] thickness must be > 1, else does nothing
 */
void R2D_Canvas_DrawThickLine(SDL_Renderer *render, int x1, int y1, int x2,
                              int y2, int thickness, int r, int g, int b, int a)
{

  if (thickness <= 1)
    return;

  vector_t vec = {.x = (x2 - x1), .y = (y2 - y1)};

  // calculate perpendicular with half-thickness in place
  vector_times_scalar(vector_perpendicular(vector_normalize(&vec)),
                      thickness / 2.0f);

  // calculate perp vertices based on thickness
  SDL_Vertex verts[4];

  verts[0].position = (SDL_FPoint){.x = x1 + vec.x, .y = y1 + vec.y};
  verts[1].position = (SDL_FPoint){.x = x2 + vec.x, .y = y2 + vec.y};
  verts[2].position = (SDL_FPoint){.x = x2 - vec.x, .y = y2 - vec.y};
  verts[3].position = (SDL_FPoint){.x = x1 - vec.x, .y = y1 - vec.y};

  // set the vertex colour
  for (register int i = 0; i < 4; i++) {
    verts[i].color = (SDL_Color){.r = r, .g = g, .b = b, .a = a};
  }

  // sub-divide the two triangles; we know this is convex
  int indices[] = {0, 1, 2, 2, 3, 0};
  SDL_RenderGeometry(render, NULL, verts, 4, indices, 6);
}

/*
 * Draw a thick rectangle on a canvas using a pre-converted RGBA colour value.
 * @param [int] thickness must be > 1, else does nothing
 */
void R2D_Canvas_DrawThickRect(SDL_Renderer *render, int x, int y, int width,
                              int height, int thickness, int r, int g, int b,
                              int a)
{
  if (thickness <= 1) {
    return;
  }

  float half_thick = thickness / 2.0f;
  SDL_Vertex verts[8];

  // all points have the same colour so
  verts[0].color = (SDL_Color){.r = r, .g = g, .b = b, .a = a};
  for (register int i = 1; i < 8; i++) {
    verts[i].color = verts[0].color;
  }

  // outer coords
  verts[0].position = (SDL_FPoint){.x = x - half_thick, .y = y - half_thick};
  verts[1].position =
      (SDL_FPoint){.x = x + width + half_thick, .y = y - half_thick};
  verts[2].position =
      (SDL_FPoint){.x = x + width + half_thick, .y = y + height + half_thick};
  verts[3].position =
      (SDL_FPoint){.x = x - half_thick, .y = y + height + half_thick};

  // inner coords
  verts[4].position = (SDL_FPoint){.x = x + half_thick, .y = y + half_thick};
  verts[5].position =
      (SDL_FPoint){.x = x + width - half_thick, .y = y + half_thick};
  verts[6].position =
      (SDL_FPoint){.x = x + width - half_thick, .y = y + height - half_thick};
  verts[7].position =
      (SDL_FPoint){.x = x + half_thick, .y = y + height - half_thick};

  int indices[] = {
      0, 4, 1, // top outer triangle
      4, 1, 5, //     inner
      1, 5, 2, // right outer
      5, 2, 6, //       inner
      2, 6, 3, // bottom outer
      6, 3, 7, //        inner
      3, 7, 0, // left outer
      7, 0, 4  //      inner
  };
  SDL_RenderGeometry(render, NULL, verts, 8, indices, 24);
}

/*
 * Draw a thick ellipse on a canvas using a pre-converted RGBA colour value.
 * @param [int] thickness must be > 1, else does nothing
 */
void R2D_Canvas_DrawThickEllipse(SDL_Renderer *render, int x, int y,
                                 float xradius, float yradius, float sectors,
                                 int thickness, int r, int g, int b, int a)
{
  if (thickness <= 1) {
    return;
  }

  // renders a thick circle by treating each segment as a monotonic quad
  // and rendering as two triangles per segment
  SDL_Vertex verts[4];

  // colour all vertices
  verts[3].color = verts[2].color = verts[1].color = verts[0].color =
      (SDL_Color){.r = r, .g = g, .b = b, .a = a};
  float half_thick = thickness / 2.0f;
  SDL_FPoint outer_radius = {.x = xradius + half_thick,
                             .y = yradius + half_thick};
  SDL_FPoint inner_radius = {.x = xradius - half_thick,
                             .y = yradius - half_thick};
  float cache_cosf = cosf(0), cache_sinf = sinf(0);
  float unit_angle = 2 * M_PI / sectors;

  int indices[] = {0, 1, 3, 3, 1, 2};

  // point at 0 radians
  verts[0].position = (SDL_FPoint){.x = x + outer_radius.x * cache_cosf,
                                   .y = y + outer_radius.y * cache_sinf};
  verts[3].position = (SDL_FPoint){.x = x + inner_radius.x * cache_cosf,
                                   .y = y + inner_radius.y * cache_sinf};
  for (float i = 1; i <= sectors; i++) {
    float angle = i * unit_angle;
    // re-use index 0 and 3 from previous calculation
    verts[1].position = verts[0].position;
    verts[2].position = verts[3].position;
    // calculate new index 0 and 3 values
    cache_cosf = cosf(angle);
    cache_sinf = sinf(angle);
    verts[0].position = (SDL_FPoint){.x = x + outer_radius.x * cache_cosf,
                                     .y = y + outer_radius.y * cache_sinf};
    verts[3].position = (SDL_FPoint){.x = x + inner_radius.x * cache_cosf,
                                     .y = y + inner_radius.y * cache_sinf};
    SDL_RenderGeometry(render, NULL, verts, 4, indices, 6);
  }
}

/*
 * Draw a thin (single pixel) ellipse on a canvas using a pre-converted RGBA
 * colour value.
 */
void R2D_Canvas_DrawThinEllipse(SDL_Renderer *render, int x, int y,
                                float xradius, float yradius, float sectors,
                                int r, int g, int b, int a)
{
  float unit_angle = 2 * M_PI / sectors;
  // point at 0 radians
  float x1 = x + xradius * cosf(0);
  float y1 = y + yradius * sinf(0);
  SDL_SetRenderDrawColor(render, r, g, b, a);
  for (float i = 1; i <= sectors; i++) {
    float angle = i * unit_angle;
    // re-use from previous calculation
    float x2 = x1;
    float y2 = y1;
    // calculate new point
    x1 = x + xradius * cosf(angle);
    y1 = y + yradius * sinf(angle);
    SDL_RenderDrawLine(render, x1, y1, x2, y2);
  }
}

/*
 * Compute the intersection between two lines represented by the input line
 * segments. Unlike a typical line segment collision detector, this function
 * will find a possible intersection point even if that point is not on either
 * of the input line "segments". This helps find where two line segments "would"
 * intersect if they were long enough, in addition to the case when the segments
 * intersect.
 * @param line1_p1
 * @param line1_p2
 * @param line2_p1
 * @param line2_p2
 * @param intersection Pointer into which to write the point of intersection.
 * @return true if an intersection is found, false if the lines are parallel
 * (collinear).
 */
static bool intersect_two_lines(const vector_t *line1_p1,
                                const vector_t *line1_p2,
                                const vector_t *line2_p1,
                                const vector_t *line2_p2,
                                vector_t *intersection)
{
  vector_t alpha = {.x = line1_p2->x - line1_p1->x,
                    .y = line1_p2->y - line1_p1->y};
  vector_t beta = {.x = line2_p1->x - line2_p2->x,
                   .y = line2_p1->y - line2_p2->y};
  float denom = (alpha.y * beta.x) - (alpha.x * beta.y);

  if (denom == 0)
    return false; // collinear

  vector_t theta = {.x = line1_p1->x - line2_p1->x,
                    .y = line1_p1->y - line2_p1->y};
  float alpha_numerator = beta.y * theta.x - (beta.x * theta.y);

  intersection->x = alpha.x;
  intersection->y = alpha.y;
  vector_times_scalar(intersection, alpha_numerator / denom);
  vector_plus_vector(intersection, line1_p1);
  return true;
}

#define POLYLINE_RENDER_NVERTS 6
#define POLYLINE_RENDER_NINDICES 6

/*
 * Draw a thick N-point polyline, with a mitre join where two
 * segments are joined.
 * @param [int] thickness must be > 1, else does nothing
 */
void R2D_Canvas_DrawThickPolyline(SDL_Renderer *render, SDL_FPoint *points,
                                  int num_points, int thickness, int r, int g,
                                  int b, int a, bool skip_first_last)
{
  if (thickness <= 1)
    return;
  if (num_points < 3)
    return;

  float thick_half = thickness / 2.0f;

  // Prepare to store the different points used map
  // the triangles that render the thick polyline
  SDL_Vertex verts[POLYLINE_RENDER_NVERTS];

  // all points have the same colour so
  for (int i = 0; i < POLYLINE_RENDER_NVERTS; i++) {
    verts[i].color = (SDL_Color){.r = r, .g = g, .b = b, .a = a};
  }

  // the indices into the vertex, always the same two triangles
  const int indices[] = {
      0, 1, 2, // left outer triangle
      0, 2, 3, // left inner triangle
  };

  // Setup the first segment
  int x1 = points[0].x, y1 = points[0].y;
  int x2 = points[1].x, y2 = points[1].y;

  // calculate the unit perpendicular for each of the line segments
  vector_t vec_one_perp = {.x = (x2 - x1), .y = (y2 - y1)};
  vector_times_scalar(vector_normalize(vector_perpendicular(&vec_one_perp)),
                      thick_half);

  // left outer bottom
  verts[0].position =
      (SDL_FPoint){.x = x1 + vec_one_perp.x, .y = y1 + vec_one_perp.y};
  // left outer top
  verts[1].position =
      (SDL_FPoint){.x = x2 + vec_one_perp.x, .y = y2 + vec_one_perp.y};
  // left inner top
  verts[2].position =
      (SDL_FPoint){.x = x2 - vec_one_perp.x, .y = y2 - vec_one_perp.y};
  // left inner bottom
  verts[3].position =
      (SDL_FPoint){.x = x1 - vec_one_perp.x, .y = y1 - vec_one_perp.y};

  //
  // go through each subsequent point to work with the two segments and
  // figure out how they join, and then render the
  // left segment.
  // then shift the vertices left to work on the next and so on
  // until we have one last segment left after the loop is done
  for (int pix = 2; pix < num_points; pix++) {

    int x3 = points[pix].x, y3 = points[pix].y;
    if (x3 == x2 && y3 == y2)
      continue;

    vector_t vec_two_perp = {.x = (x3 - x2), .y = (y3 - y2)};
    vector_times_scalar(vector_normalize(vector_perpendicular(&vec_two_perp)),
                        thick_half);

    // right inner bottom
    verts[4].position =
        (SDL_FPoint){.x = x3 - vec_two_perp.x, .y = y3 - vec_two_perp.y};
    // right outer bottom
    verts[5].position =
        (SDL_FPoint){.x = x3 + vec_two_perp.x, .y = y3 + vec_two_perp.y};

    // temporarily calculate the "right inner top" to
    // figure out the intersection with the "left inner top" which
    vector_t tmp = {.x = x2 - vec_two_perp.x, .y = y2 - vec_two_perp.y};
    // find intersection of the left/right inner lines
    bool has_intersect = intersect_two_lines(
        &verts[3].position, &verts[2].position, // left inner line
        &verts[4].position, &tmp,               // right inner line
        &verts[2].position                      // over-write with intersection
    );

    if (has_intersect) {
      // not collinear
      // adjust the "left outer top" point so that it's distance from (x2, y2)
      // is the same as the left/right "inner top" intersection we calculated
      // above
      tmp = (vector_t){.x = x2, .y = y2};
      vector_minus_vector(&tmp, &verts[2].position);
      verts[1].position = (SDL_FPoint){.x = x2 + tmp.x, .y = y2 + tmp.y};
    }
    //
    // TODO: handle degenerate case that can result in crazy long mitre join
    // point
    //
    if (pix > 2 || !skip_first_last) {
      // we only render the "left" segment of this particular segment pair
      SDL_RenderGeometry(render, NULL, verts, POLYLINE_RENDER_NVERTS, indices,
                         POLYLINE_RENDER_NINDICES);
    }

    // shift left
    // i.e. (x2, y2) becomes the first point in the next triple
    //      (x3, y3) becomes the second point
    //      and their respective calculated thick corners are shifted as well
    //      ans t
    x1 = x2;
    y1 = y2;
    x2 = x3;
    y2 = y3;
    verts[0].position = verts[1].position;
    verts[3].position = verts[2].position;
    verts[2].position = verts[4].position;
    verts[1].position = verts[5].position;
  }

  if (!skip_first_last) {
    // we render the last segment.
    SDL_RenderGeometry(render, NULL, verts, POLYLINE_RENDER_NVERTS, indices,
                       POLYLINE_RENDER_NINDICES);
  }
}
// Ruby 2D Shared functions and data

#include "ruby2d.h"

// Initalize shared data
bool R2D_diagnostics = false;

// Initialization status
static bool initted = false;


/*
 * Provide a `vasprintf()` implementation for Windows
 */
#if WINDOWS && !MINGW
int vasprintf(char **strp, const char *fmt, va_list ap) {
  int r = -1, size = _vscprintf(fmt, ap);
  if ((size >= 0) && (size < INT_MAX)) {
    *strp = (char *)malloc(size + 1);
    if (*strp) {
      r = vsnprintf(*strp, size + 1, fmt, ap);
      if (r == -1) free(*strp);
    }
  } else { *strp = 0; }
  return(r);
}
#endif


/*
 * Checks if a file exists and can be accessed
 */
bool R2D_FileExists(const char *path) {
  if (!path) return false;

  if (access(path, F_OK) != -1) {
    return true;
  } else {
    return false;
  }
}


/*
 * Logs standard messages to the console
 */
void R2D_Log(int type, const char *msg, ...) {

  // Always log if diagnostics set, or if a warning or error message
  if (R2D_diagnostics || type != R2D_INFO) {

    va_list args;
    va_start(args, msg);

    switch (type) {
      case R2D_INFO:
        printf("\033[1;36mInfo:\033[0m ");
        break;
      case R2D_WARN:
        printf("\033[1;33mWarning:\033[0m ");
        break;
      case R2D_ERROR:
        printf("\033[1;31mError:\033[0m ");
        break;
    }

    vprintf(msg, args);
    printf("\n");
    va_end(args);
  }
}


/*
 * Logs Ruby 2D errors to the console, with caller and message body
 */
void R2D_Error(const char *caller, const char *msg, ...) {
  va_list args;
  va_start(args, msg);
  char *fmsg;
  vasprintf(&fmsg, msg, args);
  R2D_Log(R2D_ERROR, "(%s) %s", caller, fmsg);
  free(fmsg);
  va_end(args);
}


/*
 * Enable/disable logging of diagnostics
 */
void R2D_Diagnostics(bool status) {
  R2D_diagnostics = status;
}


/*
 * Enable terminal colors in Windows
 */
void R2D_Windows_EnableTerminalColors() {
  #if WINDOWS
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
  #endif
}


/*
 * Initialize Ruby 2D subsystems
 */
bool R2D_Init() {
  if (initted) return true;

  // Enable terminal colors in Windows
  R2D_Windows_EnableTerminalColors();

  R2D_Log(R2D_INFO, "Initializing Ruby 2D");

  // Initialize SDL
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    R2D_Error("SDL_Init", SDL_GetError());
    return false;
  }

  // Initialize SDL_ttf
  if (TTF_Init() != 0) {
    R2D_Error("TTF_Init", TTF_GetError());
    return false;
  }

  // Initialize SDL_mixer
  int mix_flags = MIX_INIT_FLAC | MIX_INIT_OGG | MIX_INIT_MP3;
  int mix_initted = Mix_Init(mix_flags);

  // Bug in SDL2_mixer 2.0.2:
  //   Mix_Init should return OR'ed flags if successful, but returns 0 instead.
  //   Fixed in: https://hg.libsdl.org/SDL_mixer/rev/7fa15b556953
  const SDL_version *linked_version = Mix_Linked_Version();
  if (linked_version->major == 2 && linked_version->minor == 0 && linked_version->patch == 2) {
    // It's version 2.0.2, don't check for Mix_Init errors
  } else {
    if ((mix_initted&mix_flags) != mix_flags) {
      R2D_Error("Mix_Init", Mix_GetError());
    }
  }

  if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) != 0) {
    R2D_Error("Mix_OpenAudio", Mix_GetError());
    return false;
  }

  // Call `R2D_Quit` at program exit
  atexit(R2D_Quit);

  // All subsystems initted
  initted = true;
  return true;
}


/*
 * Gets the primary display's dimensions
 */
void R2D_GetDisplayDimensions(int *w, int *h) {
  R2D_Init();
  SDL_DisplayMode dm;
  SDL_GetCurrentDisplayMode(0, &dm);
  *w = dm.w;
  *h = dm.h;
}


/*
 * Quits Ruby 2D subsystems
 */
void R2D_Quit() {
  IMG_Quit();
  Mix_CloseAudio();
  Mix_Quit();
  TTF_Quit();
  SDL_Quit();
  initted = false;
}
// controllers.c

#include "ruby2d.h"

// Stores the last joystick instance ID seen by the system. These instance IDs
// are unique and increment with each new joystick connected.
static int last_intance_id = -1;


/*
 * Add controller mapping from string
 */
void R2D_AddControllerMapping(const char *map) {
  int result = SDL_GameControllerAddMapping(map);

  char guid[33];
  strncpy(guid, map, 32);

  switch (result) {
    case 1:
      R2D_Log(R2D_INFO, "Mapping added for GUID: %s", guid);
      break;
    case 0:
      R2D_Log(R2D_INFO, "Mapping updated for GUID: %s", guid);
      break;
    case -1:
      R2D_Error("SDL_GameControllerAddMapping", SDL_GetError());
      break;
  }
}


/*
 * Add controller mappings from the specified file
 */
void R2D_AddControllerMappingsFromFile(const char *path) {
  if (!R2D_FileExists(path)) {
    R2D_Log(R2D_WARN, "Controller mappings file not found: %s", path);
    return;
  }

  int mappings_added = SDL_GameControllerAddMappingsFromFile(path);
  if (mappings_added == -1) {
    R2D_Error("SDL_GameControllerAddMappingsFromFile", SDL_GetError());
  } else {
    R2D_Log(R2D_INFO, "Added %i controller mapping(s)", mappings_added);
  }
}


/*
 * Check if joystick is a controller
 */
bool R2D_IsController(SDL_JoystickID id) {
  return SDL_GameControllerFromInstanceID(id) == NULL ? false : true;
}


/*
 * Open controllers and joysticks
 */
void R2D_OpenControllers() {

  char guid_str[33];

  // Enumerate joysticks
  for (int device_index = 0; device_index < SDL_NumJoysticks(); ++device_index) {

    // Check if joystick supports SDL's game controller interface (a mapping is available)
    if (SDL_IsGameController(device_index)) {
      SDL_GameController *controller = SDL_GameControllerOpen(device_index);
      SDL_JoystickID intance_id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));

      SDL_JoystickGetGUIDString(
        SDL_JoystickGetGUID(SDL_GameControllerGetJoystick(controller)),
        guid_str, 33
      );

      if (intance_id > last_intance_id) {
        if (controller) {
          R2D_Log(R2D_INFO, "Controller #%i: %s\n      GUID: %s", intance_id, SDL_GameControllerName(controller), guid_str);
        } else {
          R2D_Log(R2D_ERROR, "Could not open controller #%i: %s", intance_id, SDL_GetError());
        }
        last_intance_id = intance_id;
      }

    // Controller interface not supported, try to open as joystick
    } else {
      SDL_Joystick *joy = SDL_JoystickOpen(device_index);
      SDL_JoystickID intance_id = SDL_JoystickInstanceID(joy);

      if (!joy) {
        R2D_Log(R2D_ERROR, "Could not open controller");
      } else if(intance_id > last_intance_id) {
        SDL_JoystickGetGUIDString(
          SDL_JoystickGetGUID(joy),
          guid_str, 33
        );
        R2D_Log(R2D_INFO,
          "Controller #%i: %s\n      GUID: %s\n      Axes: %d\n      Buttons: %d\n      Balls: %d",
          intance_id, SDL_JoystickName(joy), guid_str, SDL_JoystickNumAxes(joy),
          SDL_JoystickNumButtons(joy), SDL_JoystickNumBalls(joy)
        );
        R2D_Log(R2D_WARN, "Controller #%i does not have a mapping available", intance_id);
        last_intance_id = intance_id;
      }
    }
  }
}
// font.c

#include "ruby2d.h"


/*
 * Create a TTF_Font object given a path to a font and a size
 */
TTF_Font *R2D_FontCreateTTFFont(const char *path, int size, const char *style) {

 // Check if font file exists
  if (!R2D_FileExists(path)) {
    R2D_Error("R2D_FontCreateTTFFont", "Font file `%s` not found", path);
    return NULL;
  }

  TTF_Font *font = TTF_OpenFont(path, size);

  if (!font) {
    R2D_Error("TTF_OpenFont", TTF_GetError());
    return NULL;
  }

  if(strncmp(style, "bold", 4) == 0) {
    TTF_SetFontStyle(font, TTF_STYLE_BOLD);
  } else if(strncmp(style, "italic", 6) == 0) {
    TTF_SetFontStyle(font, TTF_STYLE_ITALIC);
  } else if(strncmp(style, "underline", 9) == 0) {
    TTF_SetFontStyle(font, TTF_STYLE_UNDERLINE);
  } else if(strncmp(style, "strikethrough", 13) == 0) {
    TTF_SetFontStyle(font, TTF_STYLE_STRIKETHROUGH);
  }

  return font;
}
// Ruby 2D OpenGL Functions

#include "ruby2d.h"

// Set to `true` to force OpenGL 2.1 (for testing)
static bool FORCE_GL2 = false;

// Flag set if using OpenGL 2.1
static bool R2D_GL2 = false;

// The orthographic projection matrix for 2D rendering.
// Elements 0 and 5 are set in R2D_GL_SetViewport.
static GLfloat orthoMatrix[16] =
  {    0,    0,     0,    0,
       0,    0,     0,    0,
       0,    0,     0,    0,
   -1.0f, 1.0f, -1.0f, 1.0f };


/*
 * Prints current GL error
 */
void R2D_GL_PrintError(const char *error) {
  R2D_Log(R2D_ERROR, "%s (%d)", error, glGetError());
}


/*
 * Print info about the current OpenGL context
 */
void R2D_GL_PrintContextInfo(R2D_Window *window) {
  R2D_Log(R2D_INFO,
    "OpenGL Context\n"
    "      GL_VENDOR: %s\n"
    "      GL_RENDERER: %s\n"
    "      GL_VERSION: %s\n"
    "      GL_SHADING_LANGUAGE_VERSION: %s",
    window->R2D_GL_VENDOR,
    window->R2D_GL_RENDERER,
    window->R2D_GL_VERSION,
    window->R2D_GL_SHADING_LANGUAGE_VERSION
  );
}


/*
 * Store info about the current OpenGL context
 */
void R2D_GL_StoreContextInfo(R2D_Window *window) {

  window->R2D_GL_VENDOR   = glGetString(GL_VENDOR);
  window->R2D_GL_RENDERER = glGetString(GL_RENDERER);
  window->R2D_GL_VERSION  = glGetString(GL_VERSION);

  // These are not defined in GLES
  #if GLES
    window->R2D_GL_MAJOR_VERSION = 0;
    window->R2D_GL_MINOR_VERSION = 0;
  #else
    glGetIntegerv(GL_MAJOR_VERSION, &window->R2D_GL_MAJOR_VERSION);
    glGetIntegerv(GL_MINOR_VERSION, &window->R2D_GL_MINOR_VERSION);
  #endif

  window->R2D_GL_SHADING_LANGUAGE_VERSION = glGetString(GL_SHADING_LANGUAGE_VERSION);
};


/*
 * Creates a shader object, loads shader string, and compiles.
 * Returns 0 if shader could not be compiled.
 */
GLuint R2D_GL_LoadShader(GLenum type, const GLchar *shaderSrc, const char *shaderName) {

  // Create the shader object
  GLuint shader = glCreateShader(type);

  if (shader == 0) {
    R2D_GL_PrintError("Failed to create shader program");
    return 0;
  }

  // Load the shader source
  glShaderSource(shader, 1, &shaderSrc, NULL);

  // Compile the shader
  glCompileShader(shader);

  // Check the compile status
  GLint compiled;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

  if (!compiled) {
    GLint infoLen = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);

    if (infoLen > 1) {
      char *infoLog = malloc(sizeof(char) * infoLen);
      glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
      printf("Error compiling shader \"%s\":\n%s\n", shaderName, infoLog);
      free(infoLog);
    }

    glDeleteShader(shader);
    return 0;
  }

  return shader;
}


/*
 * Check if shader program was linked
 */
int R2D_GL_CheckLinked(GLuint program, const char *name) {

  GLint linked;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);

  if (!linked) {
    GLint infoLen = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen);

    if (infoLen > 1) {
      char *infoLog = malloc(sizeof(char) * infoLen);
      glGetProgramInfoLog(program, infoLen, NULL, infoLog);
      printf("Error linking program `%s`: %s\n", name, infoLog);
      free(infoLog);
    }

    glDeleteProgram(program);
    return GL_FALSE;
  }

  return GL_TRUE;
}


/*
 * Calculate the viewport's scaled width and height
 */
void R2D_GL_GetViewportScale(R2D_Window *window, int *w, int *h, double *scale) {

  double s = fmin(
    window->width  / (double)window->viewport.width,
    window->height / (double)window->viewport.height
  );

  *w = window->viewport.width  * s;
  *h = window->viewport.height * s;

  if (scale) *scale = s;
}


/*
 * Sets the viewport and matrix projection
 */
void R2D_GL_SetViewport(R2D_Window *window) {

  int ortho_w = window->viewport.width;
  int ortho_h = window->viewport.height;
  int x, y, w, h;  // calculated GL viewport values

  x = 0; y = 0; w = window->width; h = window->height;

  switch (window->viewport.mode) {

    case R2D_FIXED:
      w = window->orig_width;
      h = window->orig_height;
      y = window->height - h;
      break;

    case R2D_EXPAND:
      ortho_w = w;
      ortho_h = h;
      break;

    case R2D_SCALE:
      R2D_GL_GetViewportScale(window, &w, &h, NULL);
      // Center the viewport
      x = window->width  / 2.0 - w/2.0;
      y = window->height / 2.0 - h/2.0;
      break;

    case R2D_STRETCH:
      break;
  }

  glViewport(x, y, w, h);

  // Set orthographic projection matrix
  orthoMatrix[0] =  2.0f / (GLfloat)ortho_w;
  orthoMatrix[5] = -2.0f / (GLfloat)ortho_h;

  #if GLES
    R2D_GLES_ApplyProjection(orthoMatrix);
  #else
    if (R2D_GL2) {
      R2D_GL2_ApplyProjection(ortho_w, ortho_h);
    } else {
      R2D_GL3_ApplyProjection(orthoMatrix);
    }
  #endif
}


/*
 * Initialize OpenGL
 */
int R2D_GL_Init(R2D_Window *window) {

  // Specify OpenGL contexts and set attributes
  #if GLES
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  #else
    // Use legacy OpenGL 2.1
    if (FORCE_GL2) {
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    // Request an OpenGL 3.3 forward-compatible core profile
    } else {
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    }
  #endif

  // Create and store the OpenGL context
  if (FORCE_GL2) {
    window->glcontext = NULL;
  } else {
    // Ask SDL to create an OpenGL context
    window->glcontext = SDL_GL_CreateContext(window->sdl);
  }

  // Check if a valid OpenGL context was created
  if (window->glcontext) {
    // Valid context found

    // Initialize OpenGL ES 2.0
    #if GLES
      R2D_GLES_Init();
      R2D_GL_SetViewport(window);

    // Initialize OpenGL 3.3+
    #else
      // Initialize GLEW on Windows
      #if WINDOWS
        GLenum err = glewInit();
        if (GLEW_OK != err) R2D_Error("GLEW", glewGetErrorString(err));
      #endif
      R2D_GL3_Init();
      R2D_GL_SetViewport(window);
    #endif

  // Context could not be created
  } else {

    #if GLES
      R2D_Error("GLES / SDL_GL_CreateContext", SDL_GetError());

    #else
      // Try to fallback using an OpenGL 2.1 context
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

      // Try creating the context again
      window->glcontext = SDL_GL_CreateContext(window->sdl);

      // Check if this context was created
      if (window->glcontext) {
        // Valid context found
        R2D_GL2 = true;
        R2D_GL2_Init();
        R2D_GL_SetViewport(window);

      // Could not create any OpenGL contexts, hard failure
      } else {
        R2D_Error("GL2 / SDL_GL_CreateContext", SDL_GetError());
        R2D_Log(R2D_ERROR, "An OpenGL context could not be created");
        return -1;
      }
    #endif
  }

  // Store the context and print it if diagnostics is enabled
  R2D_GL_StoreContextInfo(window);
  if (R2D_diagnostics) R2D_GL_PrintContextInfo(window);

  return 0;
}


/*
 * Creates a texture for rendering
 */
void R2D_GL_CreateTexture(GLuint *id, GLint internalFormat, GLint format, GLenum type,
                          int w, int h,
                          const GLvoid *data, GLint filter) {

  // If 0, then a new texture; generate name
  if (*id == 0) glGenTextures(1, id);

  // Bind the named texture to a texturing target
  glBindTexture(GL_TEXTURE_2D, *id);

  // Specifies the 2D texture image
  glTexImage2D(
    GL_TEXTURE_2D, 0, internalFormat, w, h,
    0, format, type, data
  );

  // Set the filtering mode
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
}


/*
 * Free a texture
 */
void R2D_GL_FreeTexture(GLuint *id) {
  if (*id != 0) {
    glDeleteTextures(1, id);
    *id = 0;
  }
}


/*
 * Draw a triangle
 */
void R2D_GL_DrawTriangle(GLfloat x1, GLfloat y1,
                         GLfloat r1, GLfloat g1, GLfloat b1, GLfloat a1,
                         GLfloat x2, GLfloat y2,
                         GLfloat r2, GLfloat g2, GLfloat b2, GLfloat a2,
                         GLfloat x3, GLfloat y3,
                         GLfloat r3, GLfloat g3, GLfloat b3, GLfloat a3) {

  #if GLES
    R2D_GLES_DrawTriangle(x1, y1, r1, g1, b1, a1,
                          x2, y2, r2, g2, b2, a2,
                          x3, y3, r3, g3, b3, a3);
  #else
    if (R2D_GL2) {
      R2D_GL2_DrawTriangle(x1, y1, r1, g1, b1, a1,
                           x2, y2, r2, g2, b2, a2,
                           x3, y3, r3, g3, b3, a3);
    } else {
      R2D_GL3_DrawTriangle(x1, y1, r1, g1, b1, a1,
                           x2, y2, r2, g2, b2, a2,
                           x3, y3, r3, g3, b3, a3);
    }
  #endif
}


/*
 * Draw a texture
 */
void R2D_GL_DrawTexture(GLfloat coordinates[], GLfloat texture_coordinates[], GLfloat color[], int texture_id) {
  #if GLES
    R2D_GLES_DrawTexture(coordinates, texture_coordinates, color, texture_id);
  #else
    if (R2D_GL2) {
      R2D_GL2_DrawTexture(coordinates, texture_coordinates, color, texture_id);
    } else {
      R2D_GL3_DrawTexture(coordinates, texture_coordinates, color, texture_id);
    }
  #endif
}


/*
 * Render and flush OpenGL buffers
 */
void R2D_GL_FlushBuffers() {
  // Only implemented in our OpenGL 3.3+ and ES 2.0 renderers
  #if GLES
    R2D_GLES_FlushBuffers();
  #else
    if (!R2D_GL2) R2D_GL3_FlushBuffers();
  #endif
}


/*
 * Clear buffers to given color values
 */
void R2D_GL_Clear(R2D_Color clr) {
  glClearColor(clr.r, clr.g, clr.b, clr.a);
  glClear(GL_COLOR_BUFFER_BIT);
}
// OpenGL 2.1

#include "ruby2d.h"

#if !GLES


/*
 * Applies the projection matrix
 */
void R2D_GL2_ApplyProjection(int w, int h) {

  // Initialize the projection matrix
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  // Multiply the current matrix with the orthographic matrix
  glOrtho(0.f, w, h, 0.f, -1.f, 1.f);

  // Initialize the model-view matrix
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}


/*
 * Initalize OpenGL
 */
int R2D_GL2_Init() {

  GLenum error = GL_NO_ERROR;

  // Enable transparency
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Check for errors
  error = glGetError();
  if (error != GL_NO_ERROR) {
    R2D_GL_PrintError("OpenGL initialization failed");
    return 1;
  } else {
    return 0;
  }
}


/*
 * Draw triangle
 */
void R2D_GL2_DrawTriangle(GLfloat x1, GLfloat y1,
                          GLfloat r1, GLfloat g1, GLfloat b1, GLfloat a1,
                          GLfloat x2, GLfloat y2,
                          GLfloat r2, GLfloat g2, GLfloat b2, GLfloat a2,
                          GLfloat x3, GLfloat y3,
                          GLfloat r3, GLfloat g3, GLfloat b3, GLfloat a3) {

  glBegin(GL_TRIANGLES);
    glColor4f(r1, g1, b1, a1); glVertex2f(x1, y1);
    glColor4f(r2, g2, b2, a2); glVertex2f(x2, y2);
    glColor4f(r3, g3, b3, a3); glVertex2f(x3, y3);
  glEnd();
}


/*
 * Draw a texture (New method with vertices pre-calculated)
 */
void R2D_GL2_DrawTexture(GLfloat coordinates[], GLfloat texture_coordinates[], GLfloat color[], int texture_id) {
  glEnable(GL_TEXTURE_2D);

  glBindTexture(GL_TEXTURE_2D, texture_id);

  glBegin(GL_QUADS);
    glColor4f(color[0], color[1], color[2], color[3]);
    glTexCoord2f(texture_coordinates[0], texture_coordinates[1]); glVertex2f(coordinates[0], coordinates[1]);
    glTexCoord2f(texture_coordinates[2], texture_coordinates[3]); glVertex2f(coordinates[2], coordinates[3]);
    glTexCoord2f(texture_coordinates[4], texture_coordinates[5]); glVertex2f(coordinates[4], coordinates[5]);
    glTexCoord2f(texture_coordinates[6], texture_coordinates[7]); glVertex2f(coordinates[6], coordinates[7]);
  glEnd();

  glDisable(GL_TEXTURE_2D);
};


#endif
// OpenGL 3.3+

#include "ruby2d.h"

#if !GLES

static GLuint vbo;  // our primary vertex buffer object (VBO)
static GLuint vboSize;  // size of the VBO in bytes
static GLfloat *vboData;  // pointer to the VBO data
static GLfloat *vboDataCurrent;  // pointer to the data for the current vertices
static GLuint vboDataIndex = 0;  // index of the current object being rendered
static GLuint vboObjCapacity = 7500;  // number of objects the VBO can store
static GLuint verticesTextureIds[7500]; // store the texture_id of each vertices
static GLuint shaderProgram;  // triangle shader program
static GLuint texShaderProgram;  // texture shader program


/*
 * Applies the projection matrix
 */
void R2D_GL3_ApplyProjection(GLfloat orthoMatrix[16]) {

  // Use the program object
  glUseProgram(shaderProgram);

  // Apply the projection matrix to the triangle shader
  glUniformMatrix4fv(
    glGetUniformLocation(shaderProgram, "u_mvpMatrix"),
    1, GL_FALSE, orthoMatrix
  );

  // Use the texture program object
  glUseProgram(texShaderProgram);

  // Apply the projection matrix to the texture shader
  glUniformMatrix4fv(
    glGetUniformLocation(texShaderProgram, "u_mvpMatrix"),
    1, GL_FALSE, orthoMatrix
  );
}


/*
 * Initalize OpenGL
 */
int R2D_GL3_Init() {

  // Enable transparency
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Vertex shader source string
  GLchar vertexSource[] =
    "#version 150 core\n"  // shader version

    "uniform mat4 u_mvpMatrix;"  // projection matrix

    // Input attributes to the vertex shader
    "in vec4 position;"  // position value
    "in vec4 color;"     // vertex color
    "in vec2 texcoord;"  // texture coordinates

    // Outputs to the fragment shader
    "out vec4 Color;"     // vertex color
    "out vec2 Texcoord;"  // texture coordinates

    "void main() {"
    // Send the color and texture coordinates right through to the fragment shader
    "  Color = color;"
    "  Texcoord = texcoord;"
    // Transform the vertex position using the projection matrix
    "  gl_Position = u_mvpMatrix * position;"
    "}";

  // Fragment shader source string
  GLchar fragmentSource[] =
    "#version 150 core\n"  // shader version
    "in vec4 Color;"       // input color from vertex shader
    "out vec4 outColor;"   // output fragment color

    "void main() {"
    "  outColor = Color;"  // pass the color right through
    "}";

  // Fragment shader source string for textures
  GLchar texFragmentSource[] =
    "#version 150 core\n"     // shader version
    "in vec4 Color;"          // input color from vertex shader
    "in vec2 Texcoord;"       // input texture coordinates
    "out vec4 outColor;"      // output fragment color
    "uniform sampler2D tex;"  // 2D texture unit

    "void main() {"
    // Apply the texture unit, texture coordinates, and color
    "  outColor = texture(tex, Texcoord) * Color;"
    "}";

  // Create a vertex array object
  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  // Create a vertex buffer object and allocate data
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  vboSize = vboObjCapacity * sizeof(GLfloat) * 8;
  vboData = (GLfloat *) malloc(vboSize);
  vboDataCurrent = vboData;

  // Create an element buffer object
  GLuint ebo;
  glGenBuffers(1, &ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

  // Load the vertex and fragment shaders
  GLuint vertexShader      = R2D_GL_LoadShader(  GL_VERTEX_SHADER,      vertexSource, "GL3 Vertex");
  GLuint fragmentShader    = R2D_GL_LoadShader(GL_FRAGMENT_SHADER,    fragmentSource, "GL3 Fragment");
  GLuint texFragmentShader = R2D_GL_LoadShader(GL_FRAGMENT_SHADER, texFragmentSource, "GL3 Texture Fragment");

  // Triangle Shader //

  // Create the shader program object
  shaderProgram = glCreateProgram();

  // Check if program was created successfully
  if (shaderProgram == 0) {
    R2D_GL_PrintError("Failed to create shader program");
    return GL_FALSE;
  }

  // Attach the shader objects to the program object
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);

  // Bind the output color variable to the fragment shader color number
  glBindFragDataLocation(shaderProgram, 0, "outColor");

  // Link the shader program
  glLinkProgram(shaderProgram);

  // Check if linked
  R2D_GL_CheckLinked(shaderProgram, "GL3 shader");

  // Specify the layout of the position vertex data...
  GLint posAttrib = glGetAttribLocation(shaderProgram, "position");
  glEnableVertexAttribArray(posAttrib);
  glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), 0);

  // ...and the color vertex data
  GLint colAttrib = glGetAttribLocation(shaderProgram, "color");
  glEnableVertexAttribArray(colAttrib);
  glVertexAttribPointer(colAttrib, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));

  // Texture Shader //

  // Create the texture shader program object
  texShaderProgram = glCreateProgram();

  // Check if program was created successfully
  if (texShaderProgram == 0) {
    R2D_GL_PrintError("Failed to create shader program");
    return GL_FALSE;
  }

  // Attach the shader objects to the program object
  glAttachShader(texShaderProgram, vertexShader);
  glAttachShader(texShaderProgram, texFragmentShader);

  // Bind the output color variable to the fragment shader color number
  glBindFragDataLocation(texShaderProgram, 0, "outColor");

  // Link the shader program
  glLinkProgram(texShaderProgram);

  // Check if linked
  R2D_GL_CheckLinked(texShaderProgram, "GL3 texture shader");

  // Specify the layout of the position vertex data...
  posAttrib = glGetAttribLocation(texShaderProgram, "position");
  glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), 0);
  glEnableVertexAttribArray(posAttrib);

  // ...and the color vertex data...
  colAttrib = glGetAttribLocation(texShaderProgram, "color");
  glVertexAttribPointer(colAttrib, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
  glEnableVertexAttribArray(colAttrib);

  // ...and the texture coordinates
  GLint texAttrib = glGetAttribLocation(texShaderProgram, "texcoord");
  glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(6 * sizeof(GLfloat)));
  glEnableVertexAttribArray(texAttrib);

  // Clean up
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
  glDeleteShader(texFragmentShader);

  // If successful, return true
  return GL_TRUE;
}


/*
 * Render the vertex buffer and reset it
 */
void R2D_GL3_FlushBuffers() {
  // Bind to the vertex buffer object and update its data
  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  glBufferData(GL_ARRAY_BUFFER, vboSize, NULL, GL_DYNAMIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GLfloat) * vboDataIndex * 8, vboData);

  GLuint verticesOffset = 0;
  GLuint lastTextureId = verticesTextureIds[0];

  for (GLuint i = 0; i <= vboDataIndex; i++) {
    if(lastTextureId != verticesTextureIds[i] || i == vboDataIndex) {
      // A texture ID of 0 represents no texture (render a triangle)
      if(lastTextureId == 0) {

        // Use the triangle shader program
        glUseProgram(shaderProgram);

      // A number other than 0 represents a texture_id
      } else {

        // Use the texture shader program
        glUseProgram(texShaderProgram);

        // Bind the texture using the provided ID
        glBindTexture(GL_TEXTURE_2D, lastTextureId);
      }

      glDrawArrays(GL_TRIANGLES, verticesOffset, i - verticesOffset);

      lastTextureId = verticesTextureIds[i];
      verticesOffset = i;
    }
  }

  // Reset the buffer object index and data pointer
  vboDataIndex = 0;
  vboDataCurrent = vboData;
}


/*
 * Draw triangle
 */
void R2D_GL3_DrawTriangle(GLfloat x1, GLfloat y1,
                          GLfloat r1, GLfloat g1, GLfloat b1, GLfloat a1,
                          GLfloat x2, GLfloat y2,
                          GLfloat r2, GLfloat g2, GLfloat b2, GLfloat a2,
                          GLfloat x3, GLfloat y3,
                          GLfloat r3, GLfloat g3, GLfloat b3, GLfloat a3) {

  // If buffer is full, flush it
  if (vboDataIndex + 3 >= vboObjCapacity) R2D_GL3_FlushBuffers();

  // Set the triangle data into a formatted array
  GLfloat vertices[24] =
    { x1, y1, r1, g1, b1, a1, 0, 0,
      x2, y2, r2, g2, b2, a2, 0, 0,
      x3, y3, r3, g3, b3, a3, 0, 0 };

  // Copy the vertex data into the current position of the buffer
  memcpy(vboDataCurrent, vertices, sizeof(vertices));

  // Increment the buffer object index and the vertex data pointer for next use
  verticesTextureIds[vboDataIndex] = verticesTextureIds[vboDataIndex + 1] = verticesTextureIds[vboDataIndex + 2] = 0;
  vboDataIndex += 3;
  vboDataCurrent = (GLfloat *)((char *)vboDataCurrent + (sizeof(GLfloat) * 24));
}


/*
 * Draw a texture (New method with vertices pre-calculated)
 */
void R2D_GL3_DrawTexture(GLfloat coordinates[], GLfloat texture_coordinates[], GLfloat color[], int texture_id) {
  // If buffer is full, flush it
  if (vboDataIndex + 6 >= vboObjCapacity) R2D_GL3_FlushBuffers();

  // There are 6 vertices for a square as we are rendering two Triangles to make up our square:
  // Triangle one: Top left, Top right, Bottom right
  // Triangle two: Bottom right, Bottom left, Top left
  GLfloat vertices[48] = {
    coordinates[0], coordinates[1], color[0], color[1], color[2], color[3], texture_coordinates[0], texture_coordinates[1],
    coordinates[2], coordinates[3], color[0], color[1], color[2], color[3], texture_coordinates[2], texture_coordinates[3],
    coordinates[4], coordinates[5], color[0], color[1], color[2], color[3], texture_coordinates[4], texture_coordinates[5],
    coordinates[4], coordinates[5], color[0], color[1], color[2], color[3], texture_coordinates[4], texture_coordinates[5],
    coordinates[6], coordinates[7], color[0], color[1], color[2], color[3], texture_coordinates[6], texture_coordinates[7],
    coordinates[0], coordinates[1], color[0], color[1], color[2], color[3], texture_coordinates[0], texture_coordinates[1],
  };

  // Copy the vertex data into the current position of the buffer
  memcpy(vboDataCurrent, vertices, sizeof(vertices));

  verticesTextureIds[vboDataIndex] = verticesTextureIds[vboDataIndex + 1] = verticesTextureIds[vboDataIndex + 2] = verticesTextureIds[vboDataIndex + 3] = verticesTextureIds[vboDataIndex + 4] = verticesTextureIds[vboDataIndex + 5] = texture_id;
  vboDataIndex += 6;
  vboDataCurrent = (GLfloat *)((char *)vboDataCurrent + (sizeof(GLfloat) * 48));
}


#endif
// OpenGL ES

#include "ruby2d.h"

#if GLES

static GLuint vbo;  // our primary vertex buffer object (VBO)
static GLuint vboSize;  // size of the VBO in bytes
static GLfloat *vboData;  // pointer to the VBO data
static GLfloat *vboDataCurrent;  // pointer to the data for the current vertices
static GLuint vboDataIndex = 0;  // index of the current object being rendered
static GLuint vboObjCapacity = 7500;  // number of objects the VBO can store
static GLuint verticesTextureIds[7500]; // store the texture_id of each vertices
static GLuint shaderProgram;  // triangle shader program
static GLuint texShaderProgram;  // texture shader program


/*
 * Applies the projection matrix
 */
void R2D_GLES_ApplyProjection(GLfloat orthoMatrix[16]) {

  // Use the program object
  glUseProgram(shaderProgram);

  // Apply the projection matrix to the triangle shader
  glUniformMatrix4fv(
    glGetUniformLocation(shaderProgram, "u_mvpMatrix"),
    1, GL_FALSE, orthoMatrix
  );

  // Use the texture program object
  glUseProgram(texShaderProgram);

  // Apply the projection matrix to the texture shader
  glUniformMatrix4fv(
    glGetUniformLocation(texShaderProgram, "u_mvpMatrix"),
    1, GL_FALSE, orthoMatrix
  );
}


/*
 * Initalize OpenGL
 */
int R2D_GLES_Init() {

  // Enable transparency
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Vertex shader source string
  GLchar vertexSource[] =
    // uniforms used by the vertex shader
    "uniform mat4 u_mvpMatrix;"   // projection matrix

    // attributes input to the vertex shader
    "attribute vec4 a_position;"  // position value
    "attribute vec4 a_color;"     // input vertex color
    "attribute vec2 a_texcoord;"  // input texture

    // varying variables, input to the fragment shader
    "varying vec4 v_color;"       // output vertex color
    "varying vec2 v_texcoord;"    // output texture

    "void main()"
    "{"
    "  v_color = a_color;"
    "  v_texcoord = a_texcoord;"
    "  gl_Position = u_mvpMatrix * a_position;"
    "}";

  // Fragment shader source string
  GLchar fragmentSource[] =
    #ifdef __EMSCRIPTEN__
    "precision mediump float;"
    #endif
    // input vertex color from vertex shader
    "varying vec4 v_color;"

    "void main()"
    "{"
    "  gl_FragColor = v_color;"
    "}";

  // Fragment shader source string for textures
  GLchar texFragmentSource[] =
    #ifdef __EMSCRIPTEN__
    "precision mediump float;"
    #endif
    // input vertex color from vertex shader
    "varying vec4 v_color;"
    "varying vec2 v_texcoord;"
    "uniform sampler2D s_texture;"

    "void main()"
    "{"
    "  gl_FragColor = texture2D(s_texture, v_texcoord) * v_color;"
    "}";

  // // Create a vertex array object
  // GLuint vao;
  // glGenVertexArrays(1, &vao);
  // glBindVertexArray(vao);

  // Create a vertex buffer object and allocate data
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  vboSize = vboObjCapacity * sizeof(GLfloat) * 8;
  vboData = (GLfloat *) malloc(vboSize);
  vboDataCurrent = vboData;

  // Create an element buffer object
  GLuint ebo;
  glGenBuffers(1, &ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

  // Load the vertex and fragment shaders
  GLuint vertexShader      = R2D_GL_LoadShader(  GL_VERTEX_SHADER,      vertexSource, "GLES Vertex");
  GLuint fragmentShader    = R2D_GL_LoadShader(GL_FRAGMENT_SHADER,    fragmentSource, "GLES Fragment");
  GLuint texFragmentShader = R2D_GL_LoadShader(GL_FRAGMENT_SHADER, texFragmentSource, "GLES Texture Fragment");

  // Triangle Shader //

  // Create the shader program object
  shaderProgram = glCreateProgram();

  // Check if program was created successfully
  if (shaderProgram == 0) {
    R2D_GL_PrintError("Failed to create shader program");
    return GL_FALSE;
  }

  // Attach the shader objects to the program object
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);

  // Link the shader program
  glLinkProgram(shaderProgram);

  // Check if linked
  R2D_GL_CheckLinked(shaderProgram, "GLES shader");

  // Specify the layout of the position vertex data...
  GLint posAttrib = glGetAttribLocation(shaderProgram, "a_position");
  glEnableVertexAttribArray(posAttrib);
  glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), 0);

  // ...and the color vertex data
  GLint colAttrib = glGetAttribLocation(shaderProgram, "a_color");
  glEnableVertexAttribArray(colAttrib);
  glVertexAttribPointer(colAttrib, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));

  // Texture Shader //

  // Create the texture shader program object
  texShaderProgram = glCreateProgram();

  // Check if program was created successfully
  if (texShaderProgram == 0) {
    R2D_GL_PrintError("Failed to create shader program");
    return GL_FALSE;
  }

  // Attach the shader objects to the program object
  glAttachShader(texShaderProgram, vertexShader);
  glAttachShader(texShaderProgram, texFragmentShader);

  // Link the shader program
  glLinkProgram(texShaderProgram);

  // Check if linked
  R2D_GL_CheckLinked(texShaderProgram, "GLES texture shader");

  // Specify the layout of the position vertex data...
  posAttrib = glGetAttribLocation(texShaderProgram, "a_position");
  glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), 0);
  glEnableVertexAttribArray(posAttrib);

  // ...and the color vertex data...
  colAttrib = glGetAttribLocation(texShaderProgram, "a_color");
  glVertexAttribPointer(colAttrib, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
  glEnableVertexAttribArray(colAttrib);

  // ...and the texture coordinates
  GLint texAttrib = glGetAttribLocation(texShaderProgram, "a_texcoord");
  glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(6 * sizeof(GLfloat)));
  glEnableVertexAttribArray(texAttrib);

  // Clean up
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
  glDeleteShader(texFragmentShader);

  // If successful, return true
  return GL_TRUE;
}


/*
 * Render the vertex buffer and reset it
 */
void R2D_GLES_FlushBuffers() {

  // Bind to the vertex buffer object and update its data
  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  glBufferData(GL_ARRAY_BUFFER, vboSize, NULL, GL_DYNAMIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GLfloat) * vboDataIndex * 8, vboData);

  GLuint verticesOffset = 0;
  GLuint lastTextureId = verticesTextureIds[0];

  for (GLuint i = 0; i <= vboDataIndex; i++) {
    if (lastTextureId != verticesTextureIds[i] || i == vboDataIndex) {
      // A texture ID of 0 represents no texture (render a triangle)
      if (lastTextureId == 0) {

        // Use the triangle shader program
        glUseProgram(shaderProgram);

      // A number other than 0 represents a texture_id
      } else {

        // Use the texture shader program
        glUseProgram(texShaderProgram);

        // Bind the texture using the provided ID
        glBindTexture(GL_TEXTURE_2D, lastTextureId);
      }

      glDrawArrays(GL_TRIANGLES, verticesOffset, i - verticesOffset);

      lastTextureId = verticesTextureIds[i];
      verticesOffset = i;
    }
  }

  // Reset the buffer object index and data pointer
  vboDataIndex = 0;
  vboDataCurrent = vboData;
}


/*
 * Draw triangle
 */
void R2D_GLES_DrawTriangle(GLfloat x1, GLfloat y1,
                           GLfloat r1, GLfloat g1, GLfloat b1, GLfloat a1,
                           GLfloat x2, GLfloat y2,
                           GLfloat r2, GLfloat g2, GLfloat b2, GLfloat a2,
                           GLfloat x3, GLfloat y3,
                           GLfloat r3, GLfloat g3, GLfloat b3, GLfloat a3) {

  // If buffer is full, flush it
  if (vboDataIndex + 3 >= vboObjCapacity) R2D_GLES_FlushBuffers();

  // Set the triangle data into a formatted array
  GLfloat vertices[24] =
    { x1, y1, r1, g1, b1, a1, 0, 0,
      x2, y2, r2, g2, b2, a2, 0, 0,
      x3, y3, r3, g3, b3, a3, 0, 0 };

  // Copy the vertex data into the current position of the buffer
  memcpy(vboDataCurrent, vertices, sizeof(vertices));

  // Increment the buffer object index and the vertex data pointer for next use
  verticesTextureIds[vboDataIndex] = verticesTextureIds[vboDataIndex + 1] = verticesTextureIds[vboDataIndex + 2] = 0;
  vboDataIndex += 3;
  vboDataCurrent = (GLfloat *)((char *)vboDataCurrent + (sizeof(GLfloat) * 24));
}


/*
 * Draw a texture (New method with vertices pre-calculated)
 */
void R2D_GLES_DrawTexture(GLfloat coordinates[], GLfloat texture_coordinates[], GLfloat color[], int texture_id) {
  // If buffer is full, flush it
  if (vboDataIndex + 6 >= vboObjCapacity) R2D_GLES_FlushBuffers();

  // There are 6 vertices for a square as we are rendering two Triangles to make up our square:
  // Triangle one: Top left, Top right, Bottom right
  // Triangle two: Bottom right, Bottom left, Top left
  GLfloat vertices[48] = {
    coordinates[0], coordinates[1], color[0], color[1], color[2], color[3], texture_coordinates[0], texture_coordinates[1],
    coordinates[2], coordinates[3], color[0], color[1], color[2], color[3], texture_coordinates[2], texture_coordinates[3],
    coordinates[4], coordinates[5], color[0], color[1], color[2], color[3], texture_coordinates[4], texture_coordinates[5],
    coordinates[4], coordinates[5], color[0], color[1], color[2], color[3], texture_coordinates[4], texture_coordinates[5],
    coordinates[6], coordinates[7], color[0], color[1], color[2], color[3], texture_coordinates[6], texture_coordinates[7],
    coordinates[0], coordinates[1], color[0], color[1], color[2], color[3], texture_coordinates[0], texture_coordinates[1],
  };

  // Copy the vertex data into the current position of the buffer
  memcpy(vboDataCurrent, vertices, sizeof(vertices));

  verticesTextureIds[vboDataIndex] = verticesTextureIds[vboDataIndex + 1] = verticesTextureIds[vboDataIndex + 2] = verticesTextureIds[vboDataIndex + 3] = verticesTextureIds[vboDataIndex + 4] = verticesTextureIds[vboDataIndex + 5] = texture_id;
  vboDataIndex += 6;
  vboDataCurrent = (GLfloat *)((char *)vboDataCurrent + (sizeof(GLfloat) * 48));
}


#endif
// image.c

#include "ruby2d.h"


SDL_Surface *R2D_CreateImageSurface(const char *path) {
  R2D_Init();

  // Check if image file exists
  if (!R2D_FileExists(path)) {
    R2D_Error("R2D_CreateImageSurface", "Image file `%s` not found", path);
    return NULL;
  }

  // Load image from file as SDL_Surface
  SDL_Surface *surface = IMG_Load(path);
  if (surface != NULL) {
    int bits_per_color = surface->format->Amask == 0 ?
      surface->format->BitsPerPixel / 3 :
      surface->format->BitsPerPixel / 4;

    if (bits_per_color < 8) {
      R2D_Log(R2D_WARN, "`%s` has less than 8 bits per color and will likely not render correctly", path, bits_per_color);
    }
  }
  return surface;
}

void R2D_ImageConvertToRGB(SDL_Surface *surface) {
  Uint32 r = surface->format->Rmask;
  Uint32 g = surface->format->Gmask;
  Uint32 a = surface->format->Amask;

  if (r&0xFF000000 || r&0xFF0000) {
    char *p = (char *)surface->pixels;
    int bpp = surface->format->BytesPerPixel;
    int w = surface->w;
    int h = surface->h;
    char tmp;
    for (int i = 0; i < bpp * w * h; i += bpp) {
      if (a&0xFF) {
        tmp = p[i];
        p[i] = p[i+3];
        p[i+3] = tmp;
      }
      if (g&0xFF0000) {
        tmp = p[i+1];
        p[i+1] = p[i+2];
        p[i+2] = tmp;
      }
      if (r&0xFF0000) {
        tmp = p[i];
        p[i] = p[i+2];
        p[i+2] = tmp;
      }
    }
  }
}
// input.c

#include "ruby2d.h"


/*
 * Get the mouse coordinates relative to the viewport
 */
void R2D_GetMouseOnViewport(R2D_Window *window, int wx, int wy, int *x, int *y) {

  double scale;  // viewport scale factor
  int w, h;      // width and height of scaled viewport

  switch (window->viewport.mode) {

    case R2D_FIXED: case R2D_EXPAND:
      *x = wx / (window->orig_width  / (double)window->viewport.width);
      *y = wy / (window->orig_height / (double)window->viewport.height);
      break;

    case R2D_SCALE:
      R2D_GL_GetViewportScale(window, &w, &h, &scale);
      *x = wx * 1 / scale - (window->width  - w) / (2.0 * scale);
      *y = wy * 1 / scale - (window->height - h) / (2.0 * scale);
      break;

    case R2D_STRETCH:
      *x = wx * window->viewport.width  / (double)window->width;
      *y = wy * window->viewport.height / (double)window->height;
      break;
  }
}


/*
 * Show the cursor over the window
 */
void R2D_ShowCursor() {
  SDL_ShowCursor(SDL_ENABLE);
}


/*
 * Hide the cursor over the window
 */
void R2D_HideCursor() {
  SDL_ShowCursor(SDL_DISABLE);
}
// music.c

#include "ruby2d.h"


/*
 * Create the music
 */
R2D_Music *R2D_CreateMusic(const char *path) {
  R2D_Init();

  // Check if music file exists
  if (!R2D_FileExists(path)) {
    R2D_Error("R2D_CreateMusic", "Music file `%s` not found", path);
    return NULL;
  }

  // Allocate the music structure
  R2D_Music *mus = (R2D_Music *) malloc(sizeof(R2D_Music));
  if (!mus) {
    R2D_Error("R2D_CreateMusic", "Out of memory!");
    return NULL;
  }

  // Load the music data from file
  mus->data = Mix_LoadMUS(path);
  if (!mus->data) {
    R2D_Error("Mix_LoadMUS", Mix_GetError());
    free(mus);
    return NULL;
  }

  // Initialize values
  mus->path = path;

  // Calculate the length of music by creating a temporary R2D_Sound object
  R2D_Sound *snd = R2D_CreateSound(path);
  mus->length = R2D_GetSoundLength(snd);
  R2D_FreeSound(snd);

  return mus;
}


/*
 * Play the music
 */
void R2D_PlayMusic(R2D_Music *mus, bool loop) {
  if (!mus) return;

  // If looping, set to -1 times; else 0
  int times = loop ? -1 : 0;

  // times: 0 == once, -1 == forever
  if (Mix_PlayMusic(mus->data, times) == -1) {
    // No music for you
    R2D_Error("R2D_PlayMusic", Mix_GetError());
  }
}


/*
 * Pause the playing music
 */
void R2D_PauseMusic() {
  Mix_PauseMusic();
}


/*
 * Resume the current music
 */
void R2D_ResumeMusic() {
  Mix_ResumeMusic();
}


/*
 * Stop the playing music; interrupts fader effects
 */
void R2D_StopMusic() {
  Mix_HaltMusic();
}


/*
 * Get the music volume
 */
int R2D_GetMusicVolume() {
  // Get music volume as percentage of maximum mix volume
  return ceil(Mix_VolumeMusic(-1) * (100.0 / MIX_MAX_VOLUME));
}


/*
 * Set the music volume a given percentage
 */
void R2D_SetMusicVolume(int volume) {
  // Set volume to be a percentage of the maximum mix volume
  Mix_VolumeMusic((volume / 100.0) * MIX_MAX_VOLUME);
}


/*
 * Fade out the playing music
 */
void R2D_FadeOutMusic(int ms) {
  Mix_FadeOutMusic(ms);
}


/*
 * Get the length of the music in seconds
 */
int R2D_GetMusicLength(R2D_Music *mus) {
  return mus->length;
}


/*
 * Free the music
 */
void R2D_FreeMusic(R2D_Music *mus) {
  if (!mus) return;
  Mix_FreeMusic(mus->data);
  free(mus);
}
// Native C extension for Ruby and MRuby

#ifndef RUBY2D_IOS_TVOS
#define RUBY2D_IOS_TVOS 0
#endif

// Ruby 2D includes
#if RUBY2D_IOS_TVOS
#else
  #include <ruby2d.h>
#endif

// Ruby includes
#if MRUBY
  #include <mruby.h>
  #include <mruby/array.h>
  #include <mruby/string.h>
  #include <mruby/variable.h>
  #include <mruby/numeric.h>
  #include <mruby/data.h>
  #include <mruby/irep.h>
#else
  #include <ruby.h>
#endif

// Define Ruby type conversions in MRuby
#if MRUBY
  // C to MRuby types
  #define INT2FIX(val)   (mrb_fixnum_value(val))
  #define INT2NUM(val)   (mrb_fixnum_value((mrb_int)(val)))
  #define UINT2NUM(val)  (INT2NUM((unsigned mrb_int)(val)))
  #define LL2NUM(val)    (INT2NUM(val))
  #define ULL2NUM(val)   (INT2NUM((unsigned __int64)(val)))
  #define DBL2NUM(val)   (mrb_float_value(mrb, (mrb_float)(val)))
  // MRuby to C types
  #define TO_PDT(val)    ((mrb_type(val) == MRB_TT_FLOAT) ? mrb_float(val) : mrb_int(mrb, (val)))
  #define FIX2INT(val)   (mrb_fixnum(val))
  #define NUM2INT(val)   ((mrb_int)TO_PDT(val))
  #define NUM2UINT(val)  ((unsigned mrb_int)TO_PDT(val))
  #define NUM2LONG(val)  (mrb_int(mrb, (val)))
  #define NUM2LL(val)    ((__int64)(TO_PDT(val)))
  #define NUM2ULL(val)   ((unsigned __int64)(TO_PDT(val)))
  #define NUM2DBL(val)   (mrb_to_flo(mrb, (val)))
  #define NUM2CHR(val)   ((mrb_string_p(val) && (RSTRING_LEN(val)>=1)) ? RSTRING_PTR(val)[0] : (char)(NUM2INT(val) & 0xff))
  // Memory management
  #define ALLOC(type)        ((type *)mrb_malloc(mrb, sizeof(type)))
  #define ALLOC_N(type, n)   ((type *)mrb_malloc(mrb, sizeof(type) * (n)))
  #define ALLOCA_N(type, n)  ((type *)alloca(sizeof(type) * (n)))
  #define MEMCMP(p1, p2, type, n)  (memcmp((p1), (p2), sizeof(type)*(n)))
#endif

// Define common types and API calls, mapping to both Ruby and MRuby APIs
#if MRUBY
  // MRuby
  #define R_VAL  mrb_value
  #define R_NIL  (mrb_nil_value())
  #define R_TRUE  (mrb_true_value())
  #define R_FALSE  (mrb_false_value())
  #define R_CLASS  struct RClass *
  #define r_iv_get(self, var)  mrb_iv_get(mrb, self, mrb_intern_lit(mrb, var))
  #define r_iv_set(self, var, val)  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, var), val)
  #define r_funcall(self, method, num_args, ...)  mrb_funcall(mrb, self, method, num_args, ##__VA_ARGS__)
  #define r_str_new(str)  mrb_str_new(mrb, str, strlen(str))
  #define r_test(val)  (mrb_test(val) == true)
  #define r_ary_entry(ary, pos)  mrb_ary_entry(ary, pos)
  #define r_ary_new()  mrb_ary_new(mrb)
  #define r_ary_push(self, val)  mrb_ary_push(mrb, self, val)
  #define r_data_wrap_struct(name, data)  mrb_obj_value(Data_Wrap_Struct(mrb, mrb->object_class, &name##_data_type, data))
  #define r_data_get_struct(self, var, mrb_type, rb_type, data)  Data_Get_Struct(mrb, r_iv_get(self, var), mrb_type, data)
  #define r_define_module(name)  mrb_define_module(mrb, name)
  #define r_define_class(module, name)  mrb_define_class_under(mrb, module, name, mrb->object_class)
  #define r_define_method(class, name, function, args)  mrb_define_method(mrb, class, name, function, args)
  #define r_define_class_method(class, name, function, args)  mrb_define_class_method(mrb, class, name, function, args)
  #define r_args_none  (MRB_ARGS_NONE())
  #define r_args_req(n)  MRB_ARGS_REQ(n)
  // Helpers
  #define r_char_to_sym(str)  mrb_symbol_value(mrb_intern_cstr(mrb, str))
#else
  // Ruby
  #define R_VAL  VALUE
  #define R_NIL  Qnil
  #define R_TRUE  Qtrue
  #define R_FALSE  Qfalse
  #define R_CLASS  R_VAL
  #define r_iv_get(self, var)  rb_iv_get(self, var)
  #define r_iv_set(self, var, val)  rb_iv_set(self, var, val)
  #define r_funcall(self, method, num_args, ...)  rb_funcall(self, rb_intern(method), num_args, ##__VA_ARGS__)
  #define r_str_new(str)  rb_str_new2(str)
  #define r_test(val)  (val != Qfalse && val != Qnil)
  #define r_ary_entry(ary, pos)  rb_ary_entry(ary, pos)
  #define r_ary_new()  rb_ary_new()
  #define r_ary_push(self, val)  rb_ary_push(self, val)
  #define r_ary_len(self)  rb_ary_len(self)
  #define r_data_wrap_struct(name, data)  Data_Wrap_Struct(rb_cObject, NULL, (free_##name), data)
  #define r_data_get_struct(self, var, mrb_type, rb_type, data)  Data_Get_Struct(r_iv_get(self, var), rb_type, data)
  #define r_define_module(name)  rb_define_module(name)
  #define r_define_class(module, name)  rb_define_class_under(module, name, rb_cObject)
  #define r_define_method(class, name, function, args)  rb_define_method(class, name, function, args)
  #define r_define_class_method(class, name, function, args)  rb_define_singleton_method(class, name, function, args)
  #define r_args_none  0
  #define r_args_req(n)  n
  // Helpers
  #define r_char_to_sym(str)  ID2SYM(rb_intern(str))
#endif

// Create the MRuby context
#if MRUBY
  static mrb_state *mrb;
#endif

// Ruby 2D interpreter window
static R_VAL ruby2d_window;

// Ruby 2D native window
static R2D_Window *ruby2d_c_window;


// Method signatures and structures for Ruby 2D classes
#if MRUBY
  static void free_sound(mrb_state *mrb, void *p_);
  static const struct mrb_data_type sound_data_type = {
    "sound", free_sound
  };
  static void free_music(mrb_state *mrb, void *p_);
  static const struct mrb_data_type music_data_type = {
    "music", free_music
  };
static void free_font(mrb_state *mrb, void *p_);
  static const struct mrb_data_type font_data_type = {
    "font", free_font
  };
static void free_surface(mrb_state *mrb, void *p_);
  static const struct mrb_data_type surface_data_type = {
    "surface", free_surface
  };
static void free_sdl_texture(mrb_state *mrb, void *p_);
  static const struct mrb_data_type sdl_texture_data_type = {
    "sdl_texture", free_sdl_texture
  };
static void free_renderer(mrb_state *mrb, void *p_);
  static const struct mrb_data_type renderer_data_type = {
    "renderer", free_renderer
  };
#else
  static void free_sound(R2D_Sound *snd);
  static void free_music(R2D_Music *mus);
  static void free_font(TTF_Font *font);
  static void free_surface(SDL_Surface *surface);
  static void free_sdl_texture(SDL_Texture *surface);
  static void free_renderer(SDL_Renderer *renderer);
#endif


/*
 * Function pointer to free the Ruby 2D native window
 */
static void free_window() {
  R2D_FreeWindow(ruby2d_c_window);
}


/*
 * Normalize controller axis values to 0.0...1.0
 */
double normalize_controller_axis(int val) {
  return val > 0 ? val / 32767.0 : val / 32768.0;
}


/*
 * Ruby2D#self.ext_base_path
 */
#if MRUBY
static R_VAL ruby2d_ext_base_path(mrb_state* mrb, R_VAL self) {
#else
static R_VAL ruby2d_ext_base_path(R_VAL self) {
#endif
  return r_str_new(SDL_GetBasePath());
}


/*
 * Ruby2D::Pixel#self.ext_draw
 */
#if MRUBY
static R_VAL ruby2d_pixel_ext_draw(mrb_state* mrb, R_VAL self) {
  mrb_value a;
  mrb_get_args(mrb, "o", &a);
#else
static R_VAL ruby2d_pixel_ext_draw(R_VAL self, R_VAL a) {
#endif
  // `a` is the array representing the pixel

  R2D_DrawQuad(
    NUM2DBL(r_ary_entry(a,  0)),  // x1
    NUM2DBL(r_ary_entry(a,  1)),  // y1
    NUM2DBL(r_ary_entry(a,  8)),  // color
    NUM2DBL(r_ary_entry(a,  9)),  // color
    NUM2DBL(r_ary_entry(a, 10)),  // color
    NUM2DBL(r_ary_entry(a, 11)),  // color

    NUM2DBL(r_ary_entry(a,  2)),  // x2
    NUM2DBL(r_ary_entry(a,  3)),  // y2
    NUM2DBL(r_ary_entry(a,  8)),  // color
    NUM2DBL(r_ary_entry(a,  9)),  // color
    NUM2DBL(r_ary_entry(a, 10)),  // color
    NUM2DBL(r_ary_entry(a, 11)),  // color

    NUM2DBL(r_ary_entry(a,  4)),  // x3
    NUM2DBL(r_ary_entry(a,  5)),  // y3
    NUM2DBL(r_ary_entry(a,  8)),  // color
    NUM2DBL(r_ary_entry(a,  9)),  // color
    NUM2DBL(r_ary_entry(a, 10)),  // color
    NUM2DBL(r_ary_entry(a, 11)),  // color

    NUM2DBL(r_ary_entry(a,  6)),  // x4
    NUM2DBL(r_ary_entry(a,  7)),  // y4
    NUM2DBL(r_ary_entry(a,  8)),  // color
    NUM2DBL(r_ary_entry(a,  9)),  // color
    NUM2DBL(r_ary_entry(a, 10)),  // color
    NUM2DBL(r_ary_entry(a, 11))   // color
  );

  return R_NIL;
}


/*
 * Ruby2D::Triangle#self.ext_draw
 */
#if MRUBY
static R_VAL ruby2d_triangle_ext_draw(mrb_state* mrb, R_VAL self) {
  mrb_value a;
  mrb_get_args(mrb, "o", &a);
#else
static R_VAL ruby2d_triangle_ext_draw(R_VAL self, R_VAL a) {
#endif
  // `a` is the array representing the triangle

  R2D_DrawTriangle(
    NUM2DBL(r_ary_entry(a,  0)),  // x1
    NUM2DBL(r_ary_entry(a,  1)),  // y1
    NUM2DBL(r_ary_entry(a,  2)),  // c1 red
    NUM2DBL(r_ary_entry(a,  3)),  // c1 green
    NUM2DBL(r_ary_entry(a,  4)),  // c1 blue
    NUM2DBL(r_ary_entry(a,  5)),  // c1 alpha

    NUM2DBL(r_ary_entry(a,  6)),  // x2
    NUM2DBL(r_ary_entry(a,  7)),  // y2
    NUM2DBL(r_ary_entry(a,  8)),  // c2 red
    NUM2DBL(r_ary_entry(a,  9)),  // c2 green
    NUM2DBL(r_ary_entry(a, 10)),  // c2 blue
    NUM2DBL(r_ary_entry(a, 11)),  // c2 alpha

    NUM2DBL(r_ary_entry(a, 12)),  // x3
    NUM2DBL(r_ary_entry(a, 13)),  // y3
    NUM2DBL(r_ary_entry(a, 14)),  // c3 red
    NUM2DBL(r_ary_entry(a, 15)),  // c3 green
    NUM2DBL(r_ary_entry(a, 16)),  // c3 blue
    NUM2DBL(r_ary_entry(a, 17))  // c3 alpha
  );

  return R_NIL;
}


/*
 * Ruby2D::Quad#self.ext_draw
 */
#if MRUBY
static R_VAL ruby2d_quad_ext_draw(mrb_state* mrb, R_VAL self) {
  mrb_value a;
  mrb_get_args(mrb, "o", &a);
#else
static R_VAL ruby2d_quad_ext_draw(R_VAL self, R_VAL a) {
#endif
  // `a` is the array representing the quad

  R2D_DrawQuad(
    NUM2DBL(r_ary_entry(a,  0)),  // x1
    NUM2DBL(r_ary_entry(a,  1)),  // y1
    NUM2DBL(r_ary_entry(a,  2)),  // c1 red
    NUM2DBL(r_ary_entry(a,  3)),  // c1 green
    NUM2DBL(r_ary_entry(a,  4)),  // c1 blue
    NUM2DBL(r_ary_entry(a,  5)),  // c1 alpha

    NUM2DBL(r_ary_entry(a,  6)),  // x2
    NUM2DBL(r_ary_entry(a,  7)),  // y2
    NUM2DBL(r_ary_entry(a,  8)),  // c2 red
    NUM2DBL(r_ary_entry(a,  9)),  // c2 green
    NUM2DBL(r_ary_entry(a, 10)),  // c2 blue
    NUM2DBL(r_ary_entry(a, 11)),  // c2 alpha

    NUM2DBL(r_ary_entry(a, 12)),  // x3
    NUM2DBL(r_ary_entry(a, 13)),  // y3
    NUM2DBL(r_ary_entry(a, 14)),  // c3 red
    NUM2DBL(r_ary_entry(a, 15)),  // c3 green
    NUM2DBL(r_ary_entry(a, 16)),  // c3 blue
    NUM2DBL(r_ary_entry(a, 17)),  // c3 alpha

    NUM2DBL(r_ary_entry(a, 18)),  // x4
    NUM2DBL(r_ary_entry(a, 19)),  // y4
    NUM2DBL(r_ary_entry(a, 20)),  // c4 red
    NUM2DBL(r_ary_entry(a, 21)),  // c4 green
    NUM2DBL(r_ary_entry(a, 22)),  // c4 blue
    NUM2DBL(r_ary_entry(a, 23))   // c4 alpha
  );

  return R_NIL;
}


/*
 * Ruby2D::Line#self.ext_draw
 */
#if MRUBY
static R_VAL ruby2d_line_ext_draw(mrb_state* mrb, R_VAL self) {
  mrb_value a;
  mrb_get_args(mrb, "o", &a);
#else
static R_VAL ruby2d_line_ext_draw(R_VAL self, R_VAL a) {
#endif
  // `a` is the array representing the line

  R2D_DrawLine(
    NUM2DBL(r_ary_entry(a,  0)),  // x1
    NUM2DBL(r_ary_entry(a,  1)),  // y1
    NUM2DBL(r_ary_entry(a,  2)),  // x2
    NUM2DBL(r_ary_entry(a,  3)),  // y2
    NUM2DBL(r_ary_entry(a,  4)),  // width

    NUM2DBL(r_ary_entry(a,  5)),  // c1 red
    NUM2DBL(r_ary_entry(a,  6)),  // c1 green
    NUM2DBL(r_ary_entry(a,  7)),  // c1 blue
    NUM2DBL(r_ary_entry(a,  8)),  // c1 alpha

    NUM2DBL(r_ary_entry(a,  9)),  // c2 red
    NUM2DBL(r_ary_entry(a, 10)),  // c2 green
    NUM2DBL(r_ary_entry(a, 11)),  // c2 blue
    NUM2DBL(r_ary_entry(a, 12)),  // c2 alpha

    NUM2DBL(r_ary_entry(a, 13)),  // c3 red
    NUM2DBL(r_ary_entry(a, 14)),  // c3 green
    NUM2DBL(r_ary_entry(a, 15)),  // c3 blue
    NUM2DBL(r_ary_entry(a, 16)),  // c3 alpha

    NUM2DBL(r_ary_entry(a, 17)),  // c4 red
    NUM2DBL(r_ary_entry(a, 18)),  // c4 green
    NUM2DBL(r_ary_entry(a, 19)),  // c4 blue
    NUM2DBL(r_ary_entry(a, 20))   // c4 alpha
  );

  return R_NIL;
}


/*
 * Ruby2D::Circle#self.ext_draw
 */
#if MRUBY
static R_VAL ruby2d_circle_ext_draw(mrb_state* mrb, R_VAL self) {
  mrb_value a;
  mrb_get_args(mrb, "o", &a);
#else
static R_VAL ruby2d_circle_ext_draw(R_VAL self, R_VAL a) {
#endif
  // `a` is the array representing the circle

  R2D_DrawCircle(
    NUM2DBL(r_ary_entry(a, 0)),  // x
    NUM2DBL(r_ary_entry(a, 1)),  // y
    NUM2DBL(r_ary_entry(a, 2)),  // radius
    NUM2DBL(r_ary_entry(a, 3)),  // sectors
    NUM2DBL(r_ary_entry(a, 4)),  // red
    NUM2DBL(r_ary_entry(a, 5)),  // green
    NUM2DBL(r_ary_entry(a, 6)),  // blue
    NUM2DBL(r_ary_entry(a, 7))   // alpha
  );

  return R_NIL;
}


/*
 * Ruby2D::Pixmap#ext_load_pixmap
 * Create an SDL surface from an image path, and ... 
 * TODO: store the surface, width, and height in the object
 */
#if MRUBY
static R_VAL ruby2d_pixmap_ext_load_pixmap(mrb_state* mrb, R_VAL self) {
  mrb_value path;
  mrb_get_args(mrb, "o", &path);
#else
static R_VAL ruby2d_pixmap_ext_load_pixmap(R_VAL self, R_VAL path) {
#endif
  R2D_Init();

  SDL_Surface *surface = R2D_CreateImageSurface(RSTRING_PTR(path));
  // initialize for later use
  r_iv_set(self, "@ext_sdl_texture", R_NIL);

  if (surface != NULL) {
#if GLES
    // OpenGL ES doesn't support BGR color order, so applying the
    // conversion to the pixels. This will however render incorrect colours
    // for BGR images when using Canvas. 
    // TODO: A better solution one day?
    R2D_ImageConvertToRGB(surface);
#endif

    r_iv_set(self, "@ext_pixel_data", r_data_wrap_struct(surface, surface));
    r_iv_set(self, "@width", INT2NUM(surface->w));
    r_iv_set(self, "@height", INT2NUM(surface->h));
  }
  else {
    // TODO: consider raising an exception?
    //       for docs: https://silverhammermba.github.io/emberb/c/#raise
    r_iv_set(self, "@ext_pixel_data", R_NIL);
    r_iv_set(self, "@width", INT2NUM(0));
    r_iv_set(self, "@height", INT2NUM(0));
  }
  return R_NIL;
}

/*
 * Ruby2D::Text#ext_load_text
 */
#if MRUBY
static R_VAL ruby2d_text_ext_load_text(mrb_state* mrb, R_VAL self) {
  mrb_value font, message;
  mrb_get_args(mrb, "oo", &font, &message);
#else
static R_VAL ruby2d_text_ext_load_text(R_VAL self, R_VAL font, R_VAL message) {
#endif
  R2D_Init();

  R_VAL result = r_ary_new();

  TTF_Font *ttf_font;

#if MRUBY
  Data_Get_Struct(mrb, font, &font_data_type, ttf_font);
#else
  Data_Get_Struct(font, TTF_Font, ttf_font);
#endif

  SDL_Surface *surface = R2D_TextCreateSurface(ttf_font, RSTRING_PTR(message));
  if (!surface) {
    return result;
  }

  r_ary_push(result, r_data_wrap_struct(surface, surface));
  r_ary_push(result, INT2NUM(surface->w));
  r_ary_push(result, INT2NUM(surface->h));

  return result;
}

/*
 * Ruby2D::Canvas#ext_draw_pixmap
 */
#if MRUBY
static R_VAL ruby2d_canvas_ext_draw_pixmap(mrb_state* mrb, R_VAL self) {
  mrb_value pixmap, src_rect, x, y, w, h;
  mrb_get_args(mrb, "oooooo", &pixmap, &src_rect, &x, &y, &w, &h);
#else
static R_VAL ruby2d_canvas_ext_draw_pixmap(R_VAL self, R_VAL pixmap, R_VAL src_rect, R_VAL x, R_VAL y, R_VAL w, R_VAL h) {
#endif

  if (r_test(pixmap)) {
    // Retrieve pixmap's external pixel data (SDL_Surface)
    SDL_Surface *pix_surface;
    r_data_get_struct(pixmap, "@ext_pixel_data", &surface_data_type, SDL_Surface, pix_surface);

    SDL_Texture *pix_sdl_tex = NULL;
    R_VAL pix_ext_sdl_tex = r_iv_get(pixmap, "@ext_sdl_texture");
    if (r_test(pix_ext_sdl_tex)) {
      r_data_get_struct(pixmap, "@ext_sdl_texture", &sdl_texture_data_type, SDL_Texture, pix_sdl_tex);
    }

    SDL_Renderer *render;
    r_data_get_struct(self, "@ext_renderer", &renderer_data_type, SDL_Renderer, render);

    if (pix_sdl_tex == NULL) {
      // create and cache an SDL_Texture for this Pixmap
      pix_sdl_tex = SDL_CreateTextureFromSurface(render, pix_surface);
      if (pix_sdl_tex != NULL) {
        r_iv_set(pixmap, "@ext_sdl_texture", r_data_wrap_struct(sdl_texture, pix_sdl_tex));
      }
      else printf("*** Unable to create SDL_Texture: %s\n", SDL_GetError());
    }

    // Draw if we have an SDL_Texture
    if (pix_sdl_tex != NULL) {
      SDL_bool src_set = SDL_FALSE;
      SDL_Rect src;
      if (r_test(src_rect)) {
        src_set = SDL_TRUE;
        // portion of pixmap
        src = (SDL_Rect) {
          .x = NUM2INT(r_ary_entry(src_rect, 0)),
          .y = NUM2INT(r_ary_entry(src_rect, 1)),
          .w = NUM2INT(r_ary_entry(src_rect, 2)),
          .h = NUM2INT(r_ary_entry(src_rect, 3))
        };
      }
      else {
        // whole pixmap
        src = (SDL_Rect){ .x = 0, .y = 0, .w = pix_surface->w, .h = pix_surface->h }; 
      }
      // use incoming size or source size or fallback to pixmap size
      int pix_w = r_test(w) ? NUM2INT(w) : (src_set ? src.w : NUM2INT(r_iv_get(pixmap, "@width")));
      int pix_h = r_test(h) ? NUM2INT(h) : (src_set ? src.h : NUM2INT(r_iv_get(pixmap, "@height")));
      
      SDL_Rect dst = { .x = NUM2INT(x), .y = NUM2INT(y), .w = pix_w, .h = pix_h };
      SDL_RenderCopy (render, pix_sdl_tex, &src, &dst);
    }
  }
  return R_NIL;
}


/*
 * Ruby2D::Texture#ext_create
 */
#if MRUBY
static R_VAL ruby2d_texture_ext_create(mrb_state* mrb, R_VAL self) {
  mrb_value rubySurface, width, height;
  mrb_get_args(mrb, "ooo", &rubySurface, &width, &height);
#else
static R_VAL ruby2d_texture_ext_create(R_VAL self, R_VAL rubySurface, R_VAL width, R_VAL height) {
#endif
  GLuint texture_id = 0;
  SDL_Surface *surface;

  #if MRUBY
    Data_Get_Struct(mrb, rubySurface, &surface_data_type, surface);
  #else
    Data_Get_Struct(rubySurface, SDL_Surface, surface);
  #endif

  // Detect image mode
  GLint gl_internal_format, gl_format;
  GLenum gl_type;  

  switch (surface->format->BytesPerPixel) {
    case 4:
#if GLES
      gl_internal_format = gl_format = GL_RGBA; 
#else
      gl_internal_format = GL_RGBA; 
      gl_format = (surface->format->Rmask == 0xff0000) ? GL_BGRA : GL_RGBA;
#endif
      gl_type = GL_UNSIGNED_BYTE;
      break;
    case 3:
#if GLES
      gl_internal_format = gl_format = GL_RGB; 
#else
      gl_internal_format = GL_RGB; 
      gl_format = (surface->format->Rmask == 0xff0000) ? GL_BGR : GL_RGB;
#endif
      gl_type = GL_UNSIGNED_BYTE;
      break;
    case 2:
      gl_internal_format = gl_format = GL_RGB;
      gl_type = GL_UNSIGNED_SHORT_5_6_5;
      break;
    case 1:
    default:
      // this would be ideal for font glyphs which use luminance + alpha and colour
      // is set when drawing
      gl_internal_format = gl_format = GL_LUMINANCE_ALPHA;
      gl_type = GL_UNSIGNED_BYTE;
      break;
  }
  R2D_GL_CreateTexture(&texture_id, gl_internal_format, gl_format, gl_type,
                       NUM2INT(width), NUM2INT(height),
                       surface->pixels, GL_NEAREST);

  return INT2NUM(texture_id);
}

/*
 * Ruby2D::Texture#ext_delete
 */
#if MRUBY
static R_VAL ruby2d_texture_ext_delete(mrb_state* mrb, R_VAL self) {
  mrb_value rubyTexture_id;
  mrb_get_args(mrb, "o", &rubyTexture_id);
#else
static R_VAL ruby2d_texture_ext_delete(R_VAL self, R_VAL rubyTexture_id) {
#endif
  GLuint texture_id = NUM2INT(rubyTexture_id);

  R2D_GL_FreeTexture(&texture_id);

  return R_NIL;
}


/*
 * Ruby2D::Canvas#ext_create
 */
#if MRUBY
static R_VAL ruby2d_canvas_ext_create(mrb_state* mrb, R_VAL self) {
  mrb_value a;
  mrb_get_args(mrb, "o", &a);
#else
static R_VAL ruby2d_canvas_ext_create(R_VAL self, R_VAL a) {
#endif
  SDL_Surface *surf = SDL_CreateRGBSurface(
    0, NUM2INT(r_ary_entry(a, 0)), NUM2INT(r_ary_entry(a, 1)),
    32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000
  );

  SDL_Renderer *render = SDL_CreateSoftwareRenderer(surf);

  SDL_SetRenderDrawColor(render,
          NUM2DBL(r_ary_entry(a, 2)) * 255, // r
          NUM2DBL(r_ary_entry(a, 3)) * 255, // g
          NUM2DBL(r_ary_entry(a, 4)) * 255, // b
          NUM2DBL(r_ary_entry(a, 5)) * 255  // a
          );
  SDL_SetRenderDrawBlendMode(render, SDL_BLENDMODE_NONE);
  SDL_RenderClear(render);

  SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawBlendMode(render, SDL_BLENDMODE_BLEND);

  r_iv_set(self, "@ext_pixel_data", r_data_wrap_struct(surface, surf));
  r_iv_set(self, "@ext_renderer", r_data_wrap_struct(renderer, render));
  return R_NIL;
}

/*
 * Ruby2D::Canvas#ext_clear
 */
#if MRUBY
static R_VAL ruby2d_canvas_ext_clear(mrb_state* mrb, R_VAL self) {
  mrb_value a;
  mrb_get_args(mrb, "o", &a);
#else
static R_VAL ruby2d_canvas_ext_clear(R_VAL self, R_VAL a) {
#endif
  // `a` is the array representing the colour values

  SDL_Renderer *render;
  r_data_get_struct(self, "@ext_renderer", &renderer_data_type, SDL_Renderer, render);

  SDL_SetRenderDrawColor(render,
          NUM2DBL(r_ary_entry(a, 0)) * 255, // r
          NUM2DBL(r_ary_entry(a, 1)) * 255, // g
          NUM2DBL(r_ary_entry(a, 2)) * 255, // b
          NUM2DBL(r_ary_entry(a, 3)) * 255  // a
          );
  SDL_SetRenderDrawBlendMode(render, SDL_BLENDMODE_NONE);
  SDL_RenderClear(render);
  SDL_SetRenderDrawBlendMode(render, SDL_BLENDMODE_BLEND);

  return R_NIL;
}

/*
 * Ruby2D::Canvas#ext_fill_rectangle
 */
#if MRUBY
static R_VAL ruby2d_canvas_ext_fill_rectangle(mrb_state* mrb, R_VAL self) {
  mrb_value a;
  mrb_get_args(mrb, "o", &a);
#else
static R_VAL ruby2d_canvas_ext_fill_rectangle(R_VAL self, R_VAL a) {
#endif
  // `a` is the array representing the rectangle

  SDL_Renderer *render;
  r_data_get_struct(self, "@ext_renderer", &renderer_data_type, SDL_Renderer, render);

  SDL_SetRenderDrawBlendMode(render, SDL_BLENDMODE_BLEND);

  SDL_Rect rect = { .x = NUM2INT(r_ary_entry(a, 0)),
                    .y = NUM2INT(r_ary_entry(a, 1)),
                    .w = NUM2INT(r_ary_entry(a, 2)),
                    .h = NUM2INT(r_ary_entry(a, 3))
                    };

  SDL_SetRenderDrawColor(render,
          NUM2DBL(r_ary_entry(a, 4)) * 255, // r
          NUM2DBL(r_ary_entry(a, 5)) * 255, // g
          NUM2DBL(r_ary_entry(a, 6)) * 255, // b
          NUM2DBL(r_ary_entry(a, 7)) * 255  // a
          );
  SDL_RenderFillRect(render, &rect);

  return R_NIL;
}

/*
 * Ruby2D::Canvas#ext_draw_rectangle
 */
#if MRUBY
static R_VAL ruby2d_canvas_ext_draw_rectangle(mrb_state* mrb, R_VAL self) {
  mrb_value a;
  mrb_get_args(mrb, "o", &a);
#else
static R_VAL ruby2d_canvas_ext_draw_rectangle(R_VAL self, R_VAL a) {
#endif
  // `a` is the array representing the rectangle
  //   0, 1,   2,     3,       4,      5, 6, 7, 8
  // [ x, y, width, height, thickness, r, g, b, a]

  SDL_Renderer *render;
  r_data_get_struct(self, "@ext_renderer", &renderer_data_type, SDL_Renderer, render);

  int thickness = NUM2INT(r_ary_entry(a, 4)); 
  if (thickness == 1) {
    SDL_Rect rect = { .x = NUM2INT(r_ary_entry(a, 0)),
                      .y = NUM2INT(r_ary_entry(a, 1)),
                      .w = NUM2INT(r_ary_entry(a, 2)),
                      .h = NUM2INT(r_ary_entry(a, 3))
                      };

    SDL_SetRenderDrawColor(render,
            NUM2DBL(r_ary_entry(a, 5)) * 255, // r
            NUM2DBL(r_ary_entry(a, 6)) * 255, // g
            NUM2DBL(r_ary_entry(a, 7)) * 255, // b
            NUM2DBL(r_ary_entry(a, 8)) * 255  // a
            );
    SDL_RenderDrawRect(render, &rect);
  }
  else if (thickness > 1) {
    R2D_Canvas_DrawThickRect(render, 
      NUM2INT(r_ary_entry(a, 0)), // x
      NUM2INT(r_ary_entry(a, 1)), // y
      NUM2INT(r_ary_entry(a, 2)), // width
      NUM2INT(r_ary_entry(a, 3)), // height
      thickness,
      NUM2DBL(r_ary_entry(a, 5)) * 255, // r
      NUM2DBL(r_ary_entry(a, 6)) * 255, // g
      NUM2DBL(r_ary_entry(a, 7)) * 255, // b
      NUM2DBL(r_ary_entry(a, 8)) * 255  // a
    );
  }
  return R_NIL;
}

/*
 * Ruby2D::Canvas#ext_draw_line
 */
#if MRUBY
static R_VAL ruby2d_canvas_ext_draw_line(mrb_state* mrb, R_VAL self) {
  mrb_value a;
  mrb_get_args(mrb, "o", &a);
#else
static R_VAL ruby2d_canvas_ext_draw_line(R_VAL self, R_VAL a) {
#endif
  // `a` is the array representing the line

  SDL_Renderer *render;
  r_data_get_struct(self, "@ext_renderer", &renderer_data_type, SDL_Renderer, render);

  int thickness = NUM2INT(r_ary_entry(a, 4)); 
  if (thickness == 1) {
    // use the SDL_Renderer's draw line for single pixel lines
    SDL_SetRenderDrawColor(render,
            NUM2DBL(r_ary_entry(a, 5)) * 255, // r
            NUM2DBL(r_ary_entry(a, 6)) * 255, // g
            NUM2DBL(r_ary_entry(a, 7)) * 255, // b
            NUM2DBL(r_ary_entry(a, 8)) * 255  // a
            );
    SDL_RenderDrawLine(render, 
      NUM2INT(r_ary_entry(a, 0)), // x1
      NUM2INT(r_ary_entry(a, 1)), // y1
      NUM2INT(r_ary_entry(a, 2)), // x2
      NUM2INT(r_ary_entry(a, 3))  // y2
    );
  }
  else if (thickness > 1) {
    // use a custom handler to convert a thick line into a 
    // quad and draw using SDL_Renderer's geometry renderer
    R2D_Canvas_DrawThickLine(render, 
      NUM2INT(r_ary_entry(a, 0)), // x1
      NUM2INT(r_ary_entry(a, 1)), // y1
      NUM2INT(r_ary_entry(a, 2)), // x2
      NUM2INT(r_ary_entry(a, 3)), // y2
      thickness,
      NUM2DBL(r_ary_entry(a, 5)) * 255, // r
      NUM2DBL(r_ary_entry(a, 6)) * 255, // g
      NUM2DBL(r_ary_entry(a, 7)) * 255, // b
      NUM2DBL(r_ary_entry(a, 8)) * 255  // a
    );
  }

  return R_NIL;
}

#define MAX_POLY_POINTS 64

/*
 * Ruby2D::Canvas#ext_draw_polyline
 */
#if MRUBY
static R_VAL ruby2d_canvas_ext_draw_polyline(mrb_state* mrb, R_VAL self) {
  mrb_value config, coords;
  mrb_get_args(mrb, "oo", &config, &coords);
#else
static R_VAL ruby2d_canvas_ext_draw_polyline(R_VAL self, R_VAL config, R_VAL coords) {
#endif
  // `config` is an array
  //       0,        1, 2, 3, 4
  //    [ thickness, r, g, b, a ]
  //
  // `coords` is an array of x, y values
  //       0,  1,  2,  3, ...
  //    [ x1, y1, x2, y2, ... ]

  SDL_Renderer *render;
  r_data_get_struct(self, "@ext_renderer", &renderer_data_type, SDL_Renderer, render);

  long coord_count = RARRAY_LEN(coords);
  int thickness = NUM2INT(r_ary_entry(config, 0)); 
  if (thickness == 1) {
    // use the SDL_Renderer's draw line for single pixel lines
    SDL_SetRenderDrawColor(render,
            NUM2DBL(r_ary_entry(config, 1)) * 255, // r
            NUM2DBL(r_ary_entry(config, 2)) * 255, // g
            NUM2DBL(r_ary_entry(config, 3)) * 255, // b
            NUM2DBL(r_ary_entry(config, 4)) * 255  // a
            );
    int x1 = NUM2INT(r_ary_entry(coords, 0));
    int y1 = NUM2INT(r_ary_entry(coords, 1));
    for (int i = 2; i < coord_count; i+= 2) {
      int x2 = NUM2INT(r_ary_entry(coords, i));
      int y2 = NUM2INT(r_ary_entry(coords, i+1));
      SDL_RenderDrawLine(render, x1, y1, x2, y2);
      x1 = x2;
      y1 = y2;
    }
  }
  else if (thickness > 1) {
    // use a custom handler to convert a thick line into a 
    // quad and draw using SDL_Renderer's geometry renderer

    SDL_FPoint points[MAX_POLY_POINTS];
    int num_points = 0;

    points[num_points++] = (SDL_FPoint) { 
      .x = NUM2INT(r_ary_entry(coords, 0)), 
      .y = NUM2INT(r_ary_entry(coords, 1))
    };
    for (int i = 2; i < coord_count && num_points < MAX_POLY_POINTS; i+= 2) {
      points[num_points++] = (SDL_FPoint) { 
        .x = NUM2INT(r_ary_entry(coords, i)), 
        .y = NUM2INT(r_ary_entry(coords, i+1)) };
    }
    R2D_Canvas_DrawThickPolyline(render,
      points, num_points,
      thickness,
      NUM2DBL(r_ary_entry(config, 1)) * 255, // r
      NUM2DBL(r_ary_entry(config, 2)) * 255, // g
      NUM2DBL(r_ary_entry(config, 3)) * 255, // b
      NUM2DBL(r_ary_entry(config, 4)) * 255, // a
      false // don't skip first/last
    );
  }

  return R_NIL;
}

/*
 * Ruby2D::Canvas#ext_draw_polygon
 */
#if MRUBY
static R_VAL ruby2d_canvas_ext_draw_polygon(mrb_state* mrb, R_VAL self) {
  mrb_value config, coords;
  mrb_get_args(mrb, "oo", &config, &coords);
#else
static R_VAL ruby2d_canvas_ext_draw_polygon(R_VAL self, R_VAL config, R_VAL coords) {
#endif
  // `config` is an array
  //       0,        1, 2, 3, 4
  //    [ thickness, r, g, b, a ]
  //
  // `coords` is an array of x, y values
  //       0,  1,  2,  3, ...
  //    [ x1, y1, x2, y2, ... ]

  SDL_Renderer *render;
  r_data_get_struct(self, "@ext_renderer", &renderer_data_type, SDL_Renderer, render);

  long coord_count = RARRAY_LEN(coords);
  int thickness = NUM2INT(r_ary_entry(config, 0)); 
  if (thickness == 1) {
    // use the SDL_Renderer's draw line for single pixel lines
    SDL_SetRenderDrawColor(render,
            NUM2DBL(r_ary_entry(config, 1)) * 255, // r
            NUM2DBL(r_ary_entry(config, 2)) * 255, // g
            NUM2DBL(r_ary_entry(config, 3)) * 255, // b
            NUM2DBL(r_ary_entry(config, 4)) * 255  // a
            );
    int x1 = NUM2INT(r_ary_entry(coords, 0));
    int y1 = NUM2INT(r_ary_entry(coords, 1));
    for (int i = 2; i < coord_count; i+= 2) {
      int x2 = NUM2INT(r_ary_entry(coords, i));
      int y2 = NUM2INT(r_ary_entry(coords, i+1));
      SDL_RenderDrawLine(render, x1, y1, x2, y2);
      x1 = x2;
      y1 = y2;
    }
    // connect back to first point to close the shape
    int x2 = NUM2INT(r_ary_entry(coords, 0));
    int y2 = NUM2INT(r_ary_entry(coords, 1));
    SDL_RenderDrawLine(render, x1, y1, x2, y2);
  }
  else if (thickness > 1) {
    // use a custom handler to convert a thick line into a 
    // quad and draw using SDL_Renderer's geometry renderer

    SDL_FPoint points[MAX_POLY_POINTS+3];  // three extra slots to points 0, 1, 2
    int num_points = 0;

    points[num_points++] = (SDL_FPoint) { 
      .x = NUM2INT(r_ary_entry(coords, 0)), 
      .y = NUM2INT(r_ary_entry(coords, 1))
    };
    for (int i = 2; i < coord_count && num_points < MAX_POLY_POINTS; i+= 2) {
      points[num_points++] = (SDL_FPoint) { 
        .x = NUM2INT(r_ary_entry(coords, i)), 
        .y = NUM2INT(r_ary_entry(coords, i+1)) };
    }
    // repeat first three coordinates at the end
    points[num_points++] = points[0];
    points[num_points++] = points[1];
    points[num_points++] = points[2];
    // use polyline draw but ask it to skip first and last segments
    // since we added three more points at the end, these produce the
    // missing segments with proper joins
    R2D_Canvas_DrawThickPolyline(render,
      points, num_points,
      thickness,
      NUM2DBL(r_ary_entry(config, 1)) * 255, // r
      NUM2DBL(r_ary_entry(config, 2)) * 255, // g
      NUM2DBL(r_ary_entry(config, 3)) * 255, // b
      NUM2DBL(r_ary_entry(config, 4)) * 255, // a
      true // skip first/last segments when drawing the polyline
    );
  }

  return R_NIL;
}

/*
 * Ruby2D::Canvas#ext_fill_polygon
 */
#if MRUBY
static R_VAL ruby2d_canvas_ext_fill_polygon(mrb_state* mrb, R_VAL self) {
  mrb_value coords, rgbas;
  mrb_get_args(mrb, "oo", &coords, &rgbas);
#else
static R_VAL ruby2d_canvas_ext_fill_polygon(R_VAL self, R_VAL coords, R_VAL rgbas) {
#endif
  // `coords` is an array of x, y values
  //       0,  1,  2,  3, ...
  //    [ x1, y1, x2, y2, ... ]
  //
  // `rgbas` is an array of r, g, b, a values
  //       0,  1,  2,  3,  4,  5,  6,  7 ...
  //    [ r1, g1, b1, a1, r2, g2, b2, a2, ... ]

  SDL_Renderer *render;
  r_data_get_struct(self, "@ext_renderer", &renderer_data_type, SDL_Renderer, render);

  long coord_len = RARRAY_LEN(coords);
  long rgbas_len = RARRAY_LEN(rgbas);

  if (coord_len >= 6 && rgbas_len > 0) {
    SDL_FPoint points[MAX_POLY_POINTS];
    int num_points = 0;

    SDL_Color colors[MAX_POLY_POINTS];
    int num_colors = 0;

    for (int i = 0; i < coord_len && num_points < MAX_POLY_POINTS; i+= 2) {
      points[num_points++] = (SDL_FPoint) { 
        .x = NUM2INT(r_ary_entry(coords, i)), 
        .y = NUM2INT(r_ary_entry(coords, i+1)) };
    }
    for (int i = 0; i < rgbas_len && num_colors < MAX_POLY_POINTS; i+= 4) {
      colors[num_colors++] = (SDL_Color) { 
        .r = NUM2DBL(r_ary_entry(rgbas, i)) * 255, // r
        .g = NUM2DBL(r_ary_entry(rgbas, i+1)) * 255, // g
        .b = NUM2DBL(r_ary_entry(rgbas, i+2)) * 255, // b
        .a = NUM2DBL(r_ary_entry(rgbas, i+3)) * 255  // a
      };
    }

    R2D_Canvas_FillPolygon(render, points, num_points, colors, num_colors);
  }

  return R_NIL;
}

/*
 * Ruby2D::Canvas#self.ext_fill_ellipse
 */
#if MRUBY
static R_VAL ruby2d_canvas_ext_fill_ellipse(mrb_state* mrb, R_VAL self) {
  mrb_value a;
  mrb_get_args(mrb, "o", &a);
#else
static R_VAL ruby2d_canvas_ext_fill_ellipse(R_VAL self, R_VAL a) {
#endif
  // `a` is the array representing the filled circle
  //   0  1     2        3        4     5  6  7  8
  // [ x, y, xradius, yradius, sectors, r, g, b, a ]
  SDL_Renderer *render;
  r_data_get_struct(self, "@ext_renderer", &renderer_data_type, SDL_Renderer, render);

  SDL_Vertex verts[3];
  // center
  float x, y;
  verts[0].position = (SDL_FPoint) { 
    .x = x = NUM2INT(r_ary_entry(a,  0)), 
    .y = y = NUM2INT(r_ary_entry(a,  1)) 
  };
  // dimensions
  float xradius = NUM2INT(r_ary_entry(a,  2));
  float yradius = NUM2INT(r_ary_entry(a,  3));
  float sectors = NUM2INT(r_ary_entry(a,  4));
  float unit_angle = 2 * M_PI / sectors;

  // colour all vertices
  verts[1].color = verts[2].color = verts[0].color = (SDL_Color) {
      .r = NUM2DBL(r_ary_entry(a, 5)) * 255,
      .g = NUM2DBL(r_ary_entry(a, 6)) * 255,
      .b = NUM2DBL(r_ary_entry(a, 7)) * 255,
      .a = NUM2DBL(r_ary_entry(a, 8)) * 255,
  };

  // point at 0 radians
  verts[1].position = (SDL_FPoint) { 
    .x = x + xradius * cosf(0), 
    .y = y + yradius * sinf(0)
  };
  for (float i = 1; i <= sectors; i++) {
    float angle = i * unit_angle;
    // re-use from previous calculation
    verts[2].position = verts[1].position;
    // calculate new point
    verts[1].position = (SDL_FPoint) { 
      .x = x + xradius * cosf(angle), 
      .y = y + yradius * sinf(angle)
    };
    SDL_RenderGeometry(render, NULL, verts, 3, NULL, 0);
  }

  return R_NIL;
}

/*
 * Ruby2D::Canvas#self.ext_draw_ellipse
 */
#if MRUBY
static R_VAL ruby2d_canvas_ext_draw_ellipse(mrb_state* mrb, R_VAL self) {
  mrb_value a;
  mrb_get_args(mrb, "o", &a);
#else
static R_VAL ruby2d_canvas_ext_draw_ellipse(R_VAL self, R_VAL a) {
#endif
  // `a` is the array representing the ellipse to draw
  //   0  1     2        3        4         5      6  7  8  9
  // [ x, y, xradius, yradius, sectors, thickness, r, g, b, a ]

  // thickness
  int thickness = NUM2INT(r_ary_entry(a,  5));
  if (thickness <= 0) return R_NIL;

  SDL_Renderer *render;
  r_data_get_struct(self, "@ext_renderer", &renderer_data_type, SDL_Renderer, render);

  // center
  float x = NUM2INT(r_ary_entry(a,  0));
  float y = NUM2INT(r_ary_entry(a,  1)) ;
  // dimensions
  float xradius = NUM2INT(r_ary_entry(a,  2));
  float yradius = NUM2INT(r_ary_entry(a,  3));
  float sectors = NUM2INT(r_ary_entry(a,  4));
  // colors
  int clr_r = NUM2DBL(r_ary_entry(a, 6)) * 255;
  int clr_g = NUM2DBL(r_ary_entry(a, 7)) * 255;
  int clr_b = NUM2DBL(r_ary_entry(a, 8)) * 255;
  int clr_a = NUM2DBL(r_ary_entry(a, 9)) * 255;
  
  if (thickness > 1) {
    R2D_Canvas_DrawThickEllipse(render, 
      x, y, xradius, yradius, sectors, thickness,
      clr_r, clr_g, clr_b, clr_a);
  }
  else {
    R2D_Canvas_DrawThinEllipse(render, 
      x, y, xradius, yradius, sectors,
      clr_r, clr_g, clr_b, clr_a);
  }

  return R_NIL;
}

/*
 * Calculate the cross product of the two lines
 * connecting the three points clock-wise. A negative value
 * means the angle "to the right" is > 180. 
 */
/* Unused
static inline int cross_product_corner(x1, y1, x2, y2, x3, y3) {
  register int vec1_x = x1 - x2;
  register int vec1_y = y1 - y2;
  register int vec2_x = x2 - x3;
  register int vec2_y = y2 - y3;

  return (vec1_x * vec2_y) - (vec1_y * vec2_x);
}
*/

/*
 * Ruby2D::Sound#ext_init
 * Initialize sound structure data
 */
#if MRUBY
static R_VAL ruby2d_sound_ext_init(mrb_state* mrb, R_VAL self) {
  mrb_value path;
  mrb_get_args(mrb, "o", &path);
#else
static R_VAL ruby2d_sound_ext_init(R_VAL self, R_VAL path) {
#endif
  R2D_Sound *snd = R2D_CreateSound(RSTRING_PTR(path));
  if (!snd) return R_FALSE;
  r_iv_set(self, "@data", r_data_wrap_struct(sound, snd));
  return R_TRUE;
}


/*
 * Ruby2D::Sound#ext_play
 */
#if MRUBY
static R_VAL ruby2d_sound_ext_play(mrb_state* mrb, R_VAL self) {
#else
static R_VAL ruby2d_sound_ext_play(R_VAL self) {
#endif
  R2D_Sound *snd;
  r_data_get_struct(self, "@data", &sound_data_type, R2D_Sound, snd);
  R2D_PlaySound(snd, r_test(r_iv_get(self, "@loop")));
  return R_NIL;
}


/*
 * Ruby2D::Sound#ext_stop
 */
#if MRUBY
static R_VAL ruby2d_sound_ext_stop(mrb_state* mrb, R_VAL self) {
#else
static R_VAL ruby2d_sound_ext_stop(R_VAL self) {
#endif
  R2D_Sound *snd;
  r_data_get_struct(self, "@data", &sound_data_type, R2D_Sound, snd);
  R2D_StopSound(snd);
  return R_NIL;
}


/*
 * Ruby2D::Sound#ext_length
 */
#if MRUBY
static R_VAL ruby2d_sound_ext_length(mrb_state* mrb, R_VAL self) {
#else
static R_VAL ruby2d_sound_ext_length(R_VAL self) {
#endif
  R2D_Sound *snd;
  r_data_get_struct(self, "@data", &sound_data_type, R2D_Sound, snd);
  return INT2NUM(R2D_GetSoundLength(snd));
}


/*
 * Free sound structure attached to Ruby 2D `Sound` class
 */
#if MRUBY
static void free_sound(mrb_state *mrb, void *p_) {
  R2D_Sound *snd = (R2D_Sound *)p_;
#else
static void free_sound(R2D_Sound *snd) {
#endif
  R2D_FreeSound(snd);
}


/*
 * Ruby2D::Sound#ext_get_volume
 */
#if MRUBY
static R_VAL ruby2d_sound_ext_get_volume(mrb_state* mrb, R_VAL self) {
#else
static R_VAL ruby2d_sound_ext_get_volume(R_VAL self) {
#endif
  R2D_Sound *snd;
  r_data_get_struct(self, "@data", &sound_data_type, R2D_Sound, snd);
  return INT2NUM(ceil(Mix_VolumeChunk(snd->data, -1) * (100.0 / MIX_MAX_VOLUME)));
}


/*
 * Ruby2D::Music#ext_set_volume
 */
#if MRUBY
static R_VAL ruby2d_sound_ext_set_volume(mrb_state* mrb, R_VAL self) {
  mrb_value volume;
  mrb_get_args(mrb, "o", &volume);
#else
static R_VAL ruby2d_sound_ext_set_volume(R_VAL self, R_VAL volume) {
#endif
  R2D_Sound *snd;
  r_data_get_struct(self, "@data", &sound_data_type, R2D_Sound, snd);
  Mix_VolumeChunk(snd->data, (NUM2INT(volume) / 100.0) * MIX_MAX_VOLUME);
  return R_NIL;
}


/*
 * Ruby2D::Sound#ext_get_mix_volume
 */
#if MRUBY
static R_VAL ruby2d_sound_ext_get_mix_volume(mrb_state* mrb, R_VAL self) {
#else
static R_VAL ruby2d_sound_ext_get_mix_volume(R_VAL self) {
#endif
  return INT2NUM(ceil(Mix_Volume(-1, -1) * (100.0 / MIX_MAX_VOLUME)));
}


/*
 * Ruby2D::Music#ext_set_mix_volume
 */
#if MRUBY
static R_VAL ruby2d_sound_ext_set_mix_volume(mrb_state* mrb, R_VAL self) {
  mrb_value volume;
  mrb_get_args(mrb, "o", &volume);
#else
static R_VAL ruby2d_sound_ext_set_mix_volume(R_VAL self, R_VAL volume) {
#endif
  Mix_Volume(-1, (NUM2INT(volume) / 100.0) * MIX_MAX_VOLUME);
  return R_NIL;
}


/*
 * Ruby2D::Music#ext_init
 * Initialize music structure data
 */
#if MRUBY
static R_VAL ruby2d_music_ext_init(mrb_state* mrb, R_VAL self) {
  mrb_value path;
  mrb_get_args(mrb, "o", &path);
#else
static R_VAL ruby2d_music_ext_init(R_VAL self, R_VAL path) {
#endif
  R2D_Music *mus = R2D_CreateMusic(RSTRING_PTR(path));
  if (!mus) return R_FALSE;
  r_iv_set(self, "@data", r_data_wrap_struct(music, mus));
  return R_TRUE;
}


/*
 * Ruby2D::Music#ext_play
 */
#if MRUBY
static R_VAL ruby2d_music_ext_play(mrb_state* mrb, R_VAL self) {
#else
static R_VAL ruby2d_music_ext_play(R_VAL self) {
#endif
  R2D_Music *mus;
  r_data_get_struct(self, "@data", &music_data_type, R2D_Music, mus);
  R2D_PlayMusic(mus, r_test(r_iv_get(self, "@loop")));
  return R_NIL;
}


/*
 * Ruby2D::Music#ext_pause
 */
#if MRUBY
static R_VAL ruby2d_music_ext_pause(mrb_state* mrb, R_VAL self) {
#else
static R_VAL ruby2d_music_ext_pause(R_VAL self) {
#endif
  R2D_PauseMusic();
  return R_NIL;
}


/*
 * Ruby2D::Music#ext_resume
 */
#if MRUBY
static R_VAL ruby2d_music_ext_resume(mrb_state* mrb, R_VAL self) {
#else
static R_VAL ruby2d_music_ext_resume(R_VAL self) {
#endif
  R2D_ResumeMusic();
  return R_NIL;
}


/*
 * Ruby2D::Music#ext_stop
 */
#if MRUBY
static R_VAL ruby2d_music_ext_stop(mrb_state* mrb, R_VAL self) {
#else
static R_VAL ruby2d_music_ext_stop(R_VAL self) {
#endif
  R2D_StopMusic();
  return R_NIL;
}


/*
 * Ruby2D::Music#ext_get_volume
 */
#if MRUBY
static R_VAL ruby2d_music_ext_get_volume(mrb_state* mrb, R_VAL self) {
#else
static R_VAL ruby2d_music_ext_get_volume(R_VAL self) {
#endif
  return INT2NUM(R2D_GetMusicVolume());
}


/*
 * Ruby2D::Music#ext_set_volume
 */
#if MRUBY
static R_VAL ruby2d_music_ext_set_volume(mrb_state* mrb, R_VAL self) {
  mrb_value volume;
  mrb_get_args(mrb, "o", &volume);
#else
static R_VAL ruby2d_music_ext_set_volume(R_VAL self, R_VAL volume) {
#endif
  R2D_SetMusicVolume(NUM2INT(volume));
  return R_NIL;
}


/*
 * Ruby2D::Music#ext_fadeout
 */
#if MRUBY
static R_VAL ruby2d_music_ext_fadeout(mrb_state* mrb, R_VAL self) {
  mrb_value ms;
  mrb_get_args(mrb, "o", &ms);
#else
static R_VAL ruby2d_music_ext_fadeout(R_VAL self, R_VAL ms) {
#endif
  R2D_FadeOutMusic(NUM2INT(ms));
  return R_NIL;
}


/*
 * Ruby2D::Music#ext_length
 */
#if MRUBY
static R_VAL ruby2d_music_ext_length(mrb_state* mrb, R_VAL self) {
#else
static R_VAL ruby2d_music_ext_length(R_VAL self) {
#endif
  R2D_Music *ms;
  r_data_get_struct(self, "@data", &music_data_type, R2D_Music, ms);
  return INT2NUM(R2D_GetMusicLength(ms));
}


/*
 * Ruby2D::Font#ext_load
 */
#if MRUBY
static R_VAL ruby2d_font_ext_load(mrb_state* mrb, R_VAL self) {
  mrb_value path, size, style;
  mrb_get_args(mrb, "ooo", &path, &size, &style);
#else
static R_VAL ruby2d_font_ext_load(R_VAL self, R_VAL path, R_VAL size, R_VAL style) {
#endif
  R2D_Init();

  TTF_Font *font = R2D_FontCreateTTFFont(RSTRING_PTR(path), NUM2INT(size), RSTRING_PTR(style));
  if (!font) {
    return R_NIL;
  }

  return r_data_wrap_struct(font, font);
}


/*
 * Ruby2D::Texture#ext_draw
 */
#if MRUBY
static R_VAL ruby2d_texture_ext_draw(mrb_state* mrb, R_VAL self) {
  mrb_value ruby_coordinates, ruby_texture_coordinates, ruby_color, texture_id;
  mrb_get_args(mrb, "oooo", &ruby_coordinates, &ruby_texture_coordinates, &ruby_color, &texture_id);
#else
static R_VAL ruby2d_texture_ext_draw(R_VAL self, R_VAL ruby_coordinates, R_VAL ruby_texture_coordinates, R_VAL ruby_color, R_VAL texture_id) {
#endif
  GLfloat coordinates[8];
  GLfloat texture_coordinates[8];
  GLfloat color[4];

  for(int i = 0; i < 8; i++) { coordinates[i] = NUM2DBL(r_ary_entry(ruby_coordinates, i)); }
  for(int i = 0; i < 8; i++) { texture_coordinates[i] = NUM2DBL(r_ary_entry(ruby_texture_coordinates, i)); }
  for(int i = 0; i < 4; i++) { color[i] = NUM2DBL(r_ary_entry(ruby_color, i)); }
  
  R2D_GL_DrawTexture(coordinates, texture_coordinates, color, NUM2INT(texture_id));

  return R_NIL;
}


/*
 * Free font structure stored in the Ruby 2D `Font` class
 */
#if MRUBY
static void free_font(mrb_state *mrb, void *p_) {
  TTF_Font *font = (TTF_Font *)p_;
#else
static void free_font(TTF_Font *font) {
#endif
  TTF_CloseFont(font);
}


/*
 * Free surface structure used within the Ruby 2D `Texture` class
 */
#if MRUBY
static void free_surface(mrb_state *mrb, void *p_) {
  SDL_Surface *surface = (SDL_Surface *)p_;
#else
static void free_surface(SDL_Surface *surface) {
#endif
  SDL_FreeSurface(surface);
}

/*
 * Free SDL texture structure used within the Ruby 2D `Pixmap` class
 */
#if MRUBY
static void free_sdl_texture(mrb_state *mrb, void *p_) {
  SDL_Texture *sdl_texure = (SDL_Texture *)p_;
#else
static void free_sdl_texture(SDL_Texture *sdl_texure) {
#endif
  SDL_DestroyTexture(sdl_texure);
}

/*
 * Free renderer structure used within the Ruby 2D `Canvas` class
 */
#if MRUBY
static void free_renderer(mrb_state *mrb, void *p_) {
  SDL_Renderer *renderer = (SDL_Renderer *)p_;
#else
static void free_renderer(SDL_Renderer *renderer) {
#endif
  SDL_DestroyRenderer(renderer);
}


/*
 * Free music structure attached to Ruby 2D `Music` class
 */
#if MRUBY
static void free_music(mrb_state *mrb, void *p_) {
  R2D_Music *mus = (R2D_Music *)p_;
#else
static void free_music(R2D_Music *mus) {
#endif
  R2D_FreeMusic(mus);
}


/*
 * Ruby 2D native `on_key` input callback function
 */
static void on_key(R2D_Event e) {

  R_VAL type;

  switch (e.type) {
    case R2D_KEY_DOWN:
      type = r_char_to_sym("down");
      break;
    case R2D_KEY_HELD:
      type = r_char_to_sym("held");
      break;
    case R2D_KEY_UP:
      type = r_char_to_sym("up");
      break;
  }

  r_funcall(ruby2d_window, "key_callback", 2, type, r_str_new(e.key));
}


/*
 * Ruby 2D native `on_mouse` input callback function
 */
void on_mouse(R2D_Event e) {

  R_VAL type = R_NIL; R_VAL button = R_NIL; R_VAL direction = R_NIL;

  switch (e.type) {
    case R2D_MOUSE_DOWN:
      // type, button, x, y
      type = r_char_to_sym("down");
      break;
    case R2D_MOUSE_UP:
      // type, button, x, y
      type = r_char_to_sym("up");
      break;
    case R2D_MOUSE_SCROLL:
      // type, direction, delta_x, delta_y
      type = r_char_to_sym("scroll");
      direction = e.direction == R2D_MOUSE_SCROLL_NORMAL ?
        r_char_to_sym("normal") : r_char_to_sym("inverted");
      break;
    case R2D_MOUSE_MOVE:
      // type, x, y, delta_x, delta_y
      type = r_char_to_sym("move");
      break;
  }

  if (e.type == R2D_MOUSE_DOWN || e.type == R2D_MOUSE_UP) {
    switch (e.button) {
      case R2D_MOUSE_LEFT:
        button = r_char_to_sym("left");
        break;
      case R2D_MOUSE_MIDDLE:
        button = r_char_to_sym("middle");
        break;
      case R2D_MOUSE_RIGHT:
        button = r_char_to_sym("right");
        break;
      case R2D_MOUSE_X1:
        button = r_char_to_sym("x1");
        break;
      case R2D_MOUSE_X2:
        button = r_char_to_sym("x2");
        break;
    }
  }

  r_funcall(
    ruby2d_window, "mouse_callback", 7, type, button, direction,
    INT2NUM(e.x), INT2NUM(e.y), INT2NUM(e.delta_x), INT2NUM(e.delta_y)
  );
}


/*
 * Ruby 2D native `on_controller` input callback function
 */
static void on_controller(R2D_Event e) {

  R_VAL type = R_NIL; R_VAL axis = R_NIL; R_VAL button = R_NIL;

  switch (e.type) {
    case R2D_AXIS:
      type = r_char_to_sym("axis");
      switch (e.axis) {
        case R2D_AXIS_LEFTX:
          axis = r_char_to_sym("left_x");
          break;
        case R2D_AXIS_LEFTY:
          axis = r_char_to_sym("left_y");
          break;
        case R2D_AXIS_RIGHTX:
          axis = r_char_to_sym("right_x");
          break;
        case R2D_AXIS_RIGHTY:
          axis = r_char_to_sym("right_y");
          break;
        case R2D_AXIS_TRIGGERLEFT:
          axis = r_char_to_sym("trigger_left");
          break;
        case R2D_AXIS_TRIGGERRIGHT:
          axis = r_char_to_sym("trigger_right");
          break;
        case R2D_AXIS_INVALID:
          axis = r_char_to_sym("invalid");
          break;
      }
      break;
    case R2D_BUTTON_DOWN: case R2D_BUTTON_UP:
      type = e.type == R2D_BUTTON_DOWN ? r_char_to_sym("button_down") : r_char_to_sym("button_up");
      switch (e.button) {
        case R2D_BUTTON_A:
          button = r_char_to_sym("a");
          break;
        case R2D_BUTTON_B:
          button = r_char_to_sym("b");
          break;
        case R2D_BUTTON_X:
          button = r_char_to_sym("x");
          break;
        case R2D_BUTTON_Y:
          button = r_char_to_sym("y");
          break;
        case R2D_BUTTON_BACK:
          button = r_char_to_sym("back");
          break;
        case R2D_BUTTON_GUIDE:
          button = r_char_to_sym("guide");
          break;
        case R2D_BUTTON_START:
          button = r_char_to_sym("start");
          break;
        case R2D_BUTTON_LEFTSTICK:
          button = r_char_to_sym("left_stick");
          break;
        case R2D_BUTTON_RIGHTSTICK:
          button = r_char_to_sym("right_stick");
          break;
        case R2D_BUTTON_LEFTSHOULDER:
          button = r_char_to_sym("left_shoulder");
          break;
        case R2D_BUTTON_RIGHTSHOULDER:
          button = r_char_to_sym("right_shoulder");
          break;
        case R2D_BUTTON_DPAD_UP:
          button = r_char_to_sym("up");
          break;
        case R2D_BUTTON_DPAD_DOWN:
          button = r_char_to_sym("down");
          break;
        case R2D_BUTTON_DPAD_LEFT:
          button = r_char_to_sym("left");
          break;
        case R2D_BUTTON_DPAD_RIGHT:
          button = r_char_to_sym("right");
          break;
        case R2D_BUTTON_INVALID:
          button = r_char_to_sym("invalid");
          break;
      }
      break;
  }

  r_funcall(
    ruby2d_window, "controller_callback", 5, INT2NUM(e.which),
    type, axis, DBL2NUM(normalize_controller_axis(e.value)), button
  );
}


/*
 * Ruby 2D native `update` callback function
 */
static void update() {

  // Set the cursor
  r_iv_set(ruby2d_window, "@mouse_x", INT2NUM(ruby2d_c_window->mouse.x));
  r_iv_set(ruby2d_window, "@mouse_y", INT2NUM(ruby2d_c_window->mouse.y));

  // Store frames
  r_iv_set(ruby2d_window, "@frames", DBL2NUM(ruby2d_c_window->frames));

  // Store frame rate
  r_iv_set(ruby2d_window, "@fps", DBL2NUM(ruby2d_c_window->fps));

  // Call update proc, `window.update`
  r_funcall(ruby2d_window, "update_callback", 0);
}


/*
 * Ruby 2D native `render` callback function
 */
static void render() {

  // Set background color
  R_VAL bc = r_iv_get(ruby2d_window, "@background");
  ruby2d_c_window->background.r = NUM2DBL(r_iv_get(bc, "@r"));
  ruby2d_c_window->background.g = NUM2DBL(r_iv_get(bc, "@g"));
  ruby2d_c_window->background.b = NUM2DBL(r_iv_get(bc, "@b"));
  ruby2d_c_window->background.a = NUM2DBL(r_iv_get(bc, "@a"));

  // Read window objects
  R_VAL objects = r_iv_get(ruby2d_window, "@objects");
  int num_objects = NUM2INT(r_funcall(objects, "length", 0));

  // Switch on each object type
  for (int i = 0; i < num_objects; ++i) {
    R_VAL el = r_ary_entry(objects, i);
    r_funcall(el, "render", 0);  // call the object's `render` function
  }

  // Call render proc, `window.render`
  r_funcall(ruby2d_window, "render_callback", 0);
}


/*
 * Ruby2D::Window#ext_diagnostics
 */
#if MRUBY
static R_VAL ruby2d_ext_diagnostics(mrb_state* mrb, R_VAL self) {
  mrb_value enable;
  mrb_get_args(mrb, "o", &enable);
#else
static R_VAL ruby2d_ext_diagnostics(R_VAL self, R_VAL enable) {
#endif
  // Set Ruby 2D native diagnostics
  R2D_Diagnostics(r_test(enable));
  return R_TRUE;
}


/*
 * Ruby2D::Window#ext_get_display_dimensions
 */
#if MRUBY
static R_VAL ruby2d_window_ext_get_display_dimensions(mrb_state* mrb, R_VAL self) {
#else
static R_VAL ruby2d_window_ext_get_display_dimensions(R_VAL self) {
#endif
  int w; int h;
  R2D_GetDisplayDimensions(&w, &h);
  r_iv_set(self, "@display_width" , INT2NUM(w));
  r_iv_set(self, "@display_height", INT2NUM(h));
  return R_NIL;
}


/*
 * Ruby2D::Window#ext_add_controller_mappings
 */
#if MRUBY
static R_VAL ruby2d_window_ext_add_controller_mappings(mrb_state* mrb, R_VAL self) {
  mrb_value path;
  mrb_get_args(mrb, "o", &path);
#else
static R_VAL ruby2d_window_ext_add_controller_mappings(R_VAL self, R_VAL path) {
#endif
  R2D_Log(R2D_INFO, "Adding controller mappings from `%s`", RSTRING_PTR(path));
  R2D_AddControllerMappingsFromFile(RSTRING_PTR(path));
  return R_NIL;
}


/*
 * Ruby2D::Window#ext_show
 */
#if MRUBY
static R_VAL ruby2d_window_ext_show(mrb_state* mrb, R_VAL self) {
#else
static R_VAL ruby2d_window_ext_show(R_VAL self) {
#endif
  ruby2d_window = self;

  // Add controller mappings from file
  r_funcall(self, "add_controller_mappings", 0);

  // Get window attributes
  char *title = RSTRING_PTR(r_iv_get(self, "@title"));
  int width   = NUM2INT(r_iv_get(self, "@width"));
  int height  = NUM2INT(r_iv_get(self, "@height"));
  int fps_cap = NUM2INT(r_iv_get(self, "@fps_cap"));

  // Get the window icon
  char *icon = NULL;
  R_VAL iv_icon = r_iv_get(self, "@icon");
  if (r_test(iv_icon)) icon = RSTRING_PTR(iv_icon);

  // Get window flags
  int flags = 0;
  if (r_test(r_iv_get(self, "@resizable"))) {
    flags = flags | R2D_RESIZABLE;
  }
  if (r_test(r_iv_get(self, "@borderless"))) {
    flags = flags | R2D_BORDERLESS;
  }
  if (r_test(r_iv_get(self, "@fullscreen"))) {
    flags = flags | R2D_FULLSCREEN;
  }
  if (r_test(r_iv_get(self, "@highdpi"))) {
    flags = flags | R2D_HIGHDPI;
  }

  // Check viewport size and set

  R_VAL vp_w = r_iv_get(self, "@viewport_width");
  int viewport_width = r_test(vp_w) ? NUM2INT(vp_w) : width;

  R_VAL vp_h = r_iv_get(self, "@viewport_height");
  int viewport_height = r_test(vp_h) ? NUM2INT(vp_h) : height;

  // Create and show window

  ruby2d_c_window = R2D_CreateWindow(
    title, width, height, update, render, flags
  );

  ruby2d_c_window->viewport.width  = viewport_width;
  ruby2d_c_window->viewport.height = viewport_height;
  ruby2d_c_window->fps_cap         = fps_cap;
  ruby2d_c_window->icon            = icon;
  ruby2d_c_window->on_key          = on_key;
  ruby2d_c_window->on_mouse        = on_mouse;
  ruby2d_c_window->on_controller   = on_controller;

  R2D_Show(ruby2d_c_window);

  atexit(free_window);
  return R_NIL;
}


/*
 * Ruby2D::Window#ext_screenshot
 */
#if MRUBY
static R_VAL ruby2d_ext_screenshot(mrb_state* mrb, R_VAL self) {
  mrb_value path;
  mrb_get_args(mrb, "o", &path);
#else
static R_VAL ruby2d_ext_screenshot(R_VAL self, R_VAL path) {
#endif
  if (ruby2d_c_window) {
    R2D_Screenshot(ruby2d_c_window, RSTRING_PTR(path));
    return path;
  } else {
    return R_FALSE;
  }
}


/*
 * Ruby2D::Window#ext_close
 */
#if MRUBY
static R_VAL ruby2d_window_ext_close(mrb_state* mrb, R_VAL self) {
#else
static R_VAL ruby2d_window_ext_close() {
#endif
  R2D_Close(ruby2d_c_window);
  return R_NIL;
}


#if MRUBY
/*
 * MRuby entry point
 */
int main() {
  // Open the MRuby environment
  mrb = mrb_open();
  if (!mrb) { /* handle error */ }

  // Load the Ruby 2D library
  mrb_load_irep(mrb, ruby2d_lib);

#else
/*
 * Ruby C extension init
 */
void Init_ruby2d() {
#endif

  // Ruby2D
  R_CLASS ruby2d_module = r_define_module("Ruby2D");

  // Ruby2D#self.ext_base_path
  r_define_class_method(ruby2d_module, "ext_base_path", ruby2d_ext_base_path, r_args_none);

  // Ruby2D::Pixel
  R_CLASS ruby2d_pixel_class = r_define_class(ruby2d_module, "Pixel");

  // Ruby2D::Pixel#self.ext_draw
  r_define_class_method(ruby2d_pixel_class, "ext_draw", ruby2d_pixel_ext_draw, r_args_req(1));

  // Ruby2D::Triangle
  R_CLASS ruby2d_triangle_class = r_define_class(ruby2d_module, "Triangle");

  // Ruby2D::Triangle#self.ext_draw
  r_define_class_method(ruby2d_triangle_class, "ext_draw", ruby2d_triangle_ext_draw, r_args_req(1));

  // Ruby2D::Quad
  R_CLASS ruby2d_quad_class = r_define_class(ruby2d_module, "Quad");

  // Ruby2D::Quad#self.ext_draw
  r_define_class_method(ruby2d_quad_class, "ext_draw", ruby2d_quad_ext_draw, r_args_req(1));

  // Ruby2D::Line
  R_CLASS ruby2d_line_class = r_define_class(ruby2d_module, "Line");

  // Ruby2D::Line#self.ext_draw
  r_define_class_method(ruby2d_line_class, "ext_draw", ruby2d_line_ext_draw, r_args_req(1));

  // Ruby2D::Circle
  R_CLASS ruby2d_circle_class = r_define_class(ruby2d_module, "Circle");

  // Ruby2D::Circle#self.ext_draw
  r_define_class_method(ruby2d_circle_class, "ext_draw", ruby2d_circle_ext_draw, r_args_req(1));

  // Ruby2D::Pixmap
  R_CLASS ruby2d_pixmap_class = r_define_class(ruby2d_module, "Pixmap");

  // Ruby2D::Pixmap#ext_load_pixmap
  r_define_method(ruby2d_pixmap_class, "ext_load_pixmap", ruby2d_pixmap_ext_load_pixmap, r_args_req(1));

  // Ruby2D::Text
  R_CLASS ruby2d_text_class = r_define_class(ruby2d_module, "Text");

  // Ruby2D::Text#ext_load_text
  r_define_class_method(ruby2d_text_class, "ext_load_text", ruby2d_text_ext_load_text, r_args_req(2));

  // Ruby2D::Sound
  R_CLASS ruby2d_sound_class = r_define_class(ruby2d_module, "Sound");

  // Ruby2D::Sound#ext_init
  r_define_method(ruby2d_sound_class, "ext_init", ruby2d_sound_ext_init, r_args_req(1));

  // Ruby2D::Sound#ext_play
  r_define_method(ruby2d_sound_class, "ext_play", ruby2d_sound_ext_play, r_args_none);
  
  // Ruby2D::Sound#ext_stop
  r_define_method(ruby2d_sound_class, "ext_stop", ruby2d_sound_ext_stop, r_args_none);

  // Ruby2D::Sound#ext_get_volume
  r_define_method(ruby2d_sound_class, "ext_get_volume", ruby2d_sound_ext_get_volume, r_args_none);
  
  // Ruby2D::Sound#ext_set_volume
  r_define_method(ruby2d_sound_class, "ext_set_volume", ruby2d_sound_ext_set_volume, r_args_req(1));
  
  // Ruby2D::Sound#self.ext_get_mix_volume
  r_define_class_method(ruby2d_sound_class, "ext_get_mix_volume", ruby2d_sound_ext_get_mix_volume, r_args_none);
  
  // Ruby2D::Sound#self.ext_set_mix_volume
  r_define_class_method(ruby2d_sound_class, "ext_set_mix_volume", ruby2d_sound_ext_set_mix_volume, r_args_req(1));

  // Ruby2D::Sound#ext_length
  r_define_method(ruby2d_sound_class, "ext_length", ruby2d_sound_ext_length, r_args_none);

  // Ruby2D::Music
  R_CLASS ruby2d_music_class = r_define_class(ruby2d_module, "Music");

  // Ruby2D::Music#ext_init
  r_define_method(ruby2d_music_class, "ext_init", ruby2d_music_ext_init, r_args_req(1));

  // Ruby2D::Music#ext_play
  r_define_method(ruby2d_music_class, "ext_play", ruby2d_music_ext_play, r_args_none);

  // Ruby2D::Music#ext_pause
  r_define_method(ruby2d_music_class, "ext_pause", ruby2d_music_ext_pause, r_args_none);

  // Ruby2D::Music#ext_resume
  r_define_method(ruby2d_music_class, "ext_resume", ruby2d_music_ext_resume, r_args_none);

  // Ruby2D::Music#ext_stop
  r_define_method(ruby2d_music_class, "ext_stop", ruby2d_music_ext_stop, r_args_none);

  // Ruby2D::Music#self.ext_get_volume
  r_define_class_method(ruby2d_music_class, "ext_get_volume", ruby2d_music_ext_get_volume, r_args_none);

  // Ruby2D::Music#self.ext_set_volume
  r_define_class_method(ruby2d_music_class, "ext_set_volume", ruby2d_music_ext_set_volume, r_args_req(1));

  // Ruby2D::Music#ext_fadeout
  r_define_method(ruby2d_music_class, "ext_fadeout", ruby2d_music_ext_fadeout, r_args_req(1));

  // Ruby2D::Music#ext_length
  r_define_method(ruby2d_music_class, "ext_length", ruby2d_music_ext_length, r_args_none);

  // Ruby2D::Font
  R_CLASS ruby2d_font_class = r_define_class(ruby2d_module, "Font");

  // Ruby2D::Font#ext_load
  r_define_class_method(ruby2d_font_class, "ext_load", ruby2d_font_ext_load, r_args_req(3));

  // Ruby2D::Texture
  R_CLASS ruby2d_texture_class = r_define_class(ruby2d_module, "Texture");

  // Ruby2D::Texture#ext_draw
  r_define_method(ruby2d_texture_class, "ext_draw", ruby2d_texture_ext_draw, r_args_req(4));

  // Ruby2D::Texture#ext_create
  r_define_method(ruby2d_texture_class, "ext_create", ruby2d_texture_ext_create, r_args_req(3));

  // Ruby2D::Texture#ext_delete
  r_define_method(ruby2d_texture_class, "ext_delete", ruby2d_texture_ext_delete, r_args_req(1));

  // Ruby2D::Canvas
  R_CLASS ruby2d_canvas_class = r_define_class(ruby2d_module, "Canvas");

  // Ruby2D::Canvas#ext_create
  r_define_method(ruby2d_canvas_class, "ext_create", ruby2d_canvas_ext_create, r_args_req(1));

  // Ruby2D::Canvas#ext_clear
  r_define_method(ruby2d_canvas_class, "ext_clear", ruby2d_canvas_ext_clear, r_args_req(1));

  // Ruby2D::Canvas#ext_fill_rectangle
  r_define_method(ruby2d_canvas_class, "ext_fill_rectangle", ruby2d_canvas_ext_fill_rectangle, r_args_req(1));

  // Ruby2D::Canvas#ext_draw_rectangle
  r_define_method(ruby2d_canvas_class, "ext_draw_rectangle", ruby2d_canvas_ext_draw_rectangle, r_args_req(1));

  // Ruby2D::Canvas#ext_draw_line
  r_define_method(ruby2d_canvas_class, "ext_draw_line", ruby2d_canvas_ext_draw_line, r_args_req(1));

  // Ruby2D::Canvas#ext_draw_polyline
  r_define_method(ruby2d_canvas_class, "ext_draw_polyline", ruby2d_canvas_ext_draw_polyline, r_args_req(2));

  // Ruby2D::Canvas#ext_draw_polygon
  r_define_method(ruby2d_canvas_class, "ext_draw_polygon", ruby2d_canvas_ext_draw_polygon, r_args_req(2));

  // Ruby2D::Canvas#ext_fill_polygon
  r_define_method(ruby2d_canvas_class, "ext_fill_polygon", ruby2d_canvas_ext_fill_polygon, r_args_req(2));

  // Ruby2D::Canvas#ext_fill_ellipse
  r_define_method(ruby2d_canvas_class, "ext_fill_ellipse", ruby2d_canvas_ext_fill_ellipse, r_args_req(1));

  // Ruby2D::Canvas#ext_draw_ellipse
  r_define_method(ruby2d_canvas_class, "ext_draw_ellipse", ruby2d_canvas_ext_draw_ellipse, r_args_req(1));

  // Ruby2D::Canvas#ext_draw_pixmap
  r_define_method(ruby2d_canvas_class, "ext_draw_pixmap", ruby2d_canvas_ext_draw_pixmap, r_args_req(6));

  // Ruby2D::Window
  R_CLASS ruby2d_window_class = r_define_class(ruby2d_module, "Window");

  // Ruby2D::Window#ext_diagnostics
  r_define_method(ruby2d_window_class, "ext_diagnostics", ruby2d_ext_diagnostics, r_args_req(1));

  // Ruby2D::Window#ext_get_display_dimensions
  r_define_method(ruby2d_window_class, "ext_get_display_dimensions", ruby2d_window_ext_get_display_dimensions, r_args_none);

  // Ruby2D::Window#ext_add_controller_mappings
  r_define_method(ruby2d_window_class, "ext_add_controller_mappings", ruby2d_window_ext_add_controller_mappings, r_args_req(1));

  // Ruby2D::Window#ext_show
  r_define_method(ruby2d_window_class, "ext_show", ruby2d_window_ext_show, r_args_none);

  // Ruby2D::Window#ext_screenshot
  r_define_method(ruby2d_window_class, "ext_screenshot", ruby2d_ext_screenshot, r_args_req(1));

  // Ruby2D::Window#ext_close
  r_define_method(ruby2d_window_class, "ext_close", ruby2d_window_ext_close, r_args_none);

#if MRUBY
  // Load the Ruby 2D app
  mrb_load_irep(mrb, ruby2d_app);

  // If an exception, print error
  if (mrb->exc) mrb_print_error(mrb);

  // Close the MRuby environment
  mrb_close(mrb);

  return 0;
#endif
}
// shapes.c

#include "ruby2d.h"


/*
 * Rotate a point around a given point
 * Params:
 *   p      The point to rotate
 *   angle  The angle in degrees
 *   rx     The x coordinate to rotate around
 *   ry     The y coordinate to rotate around
 */
R2D_GL_Point R2D_RotatePoint(R2D_GL_Point p, GLfloat angle, GLfloat rx, GLfloat ry) {

  // Convert from degrees to radians
  angle = angle * M_PI / 180.0;

  // Get the sine and cosine of the angle
  GLfloat sa = sin(angle);
  GLfloat ca = cos(angle);

  // Translate point to origin
  p.x -= rx;
  p.y -= ry;

  // Rotate point
  GLfloat xnew = p.x * ca - p.y * sa;
  GLfloat ynew = p.x * sa + p.y * ca;

  // Translate point back
  p.x = xnew + rx;
  p.y = ynew + ry;

  return p;
}


/*
 * Get the point to be rotated around given a position in a rectangle
 */
R2D_GL_Point R2D_GetRectRotationPoint(int x, int y, int w, int h, int position) {

  R2D_GL_Point p;

  switch (position) {
    case R2D_CENTER:
      p.x = x + (w / 2.0);
      p.y = y + (h / 2.0);
      break;
    case R2D_TOP_LEFT:
      p.x = x;
      p.y = y;
      break;
    case R2D_TOP_RIGHT:
      p.x = x + w;
      p.y = y;
      break;
    case R2D_BOTTOM_LEFT:
      p.x = x;
      p.y = y + h;
      break;
    case R2D_BOTTOM_RIGHT:
      p.x = x + w;
      p.y = y + h;
      break;
  }

  return p;
}


/*
 * Draw a triangle
 */
void R2D_DrawTriangle(GLfloat x1, GLfloat y1,
                      GLfloat r1, GLfloat g1, GLfloat b1, GLfloat a1,
                      GLfloat x2, GLfloat y2,
                      GLfloat r2, GLfloat g2, GLfloat b2, GLfloat a2,
                      GLfloat x3, GLfloat y3,
                      GLfloat r3, GLfloat g3, GLfloat b3, GLfloat a3) {

  R2D_GL_DrawTriangle(x1, y1, r1, g1, b1, a1,
                      x2, y2, r2, g2, b2, a2,
                      x3, y3, r3, g3, b3, a3);
}


/*
 * Draw a quad, using two triangles
 */
void R2D_DrawQuad(GLfloat x1, GLfloat y1,
                  GLfloat r1, GLfloat g1, GLfloat b1, GLfloat a1,
                  GLfloat x2, GLfloat y2,
                  GLfloat r2, GLfloat g2, GLfloat b2, GLfloat a2,
                  GLfloat x3, GLfloat y3,
                  GLfloat r3, GLfloat g3, GLfloat b3, GLfloat a3,
                  GLfloat x4, GLfloat y4,
                  GLfloat r4, GLfloat g4, GLfloat b4, GLfloat a4) {

  R2D_GL_DrawTriangle(x1, y1, r1, g1, b1, a1,
                      x2, y2, r2, g2, b2, a2,
                      x3, y3, r3, g3, b3, a3);

  R2D_GL_DrawTriangle(x3, y3, r3, g3, b3, a3,
                      x4, y4, r4, g4, b4, a4,
                      x1, y1, r1, g1, b1, a1);
};


/*
 * Draw a line from a quad
 */
void R2D_DrawLine(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2,
                  GLfloat width,
                  GLfloat r1, GLfloat g1, GLfloat b1, GLfloat a1,
                  GLfloat r2, GLfloat g2, GLfloat b2, GLfloat a2,
                  GLfloat r3, GLfloat g3, GLfloat b3, GLfloat a3,
                  GLfloat r4, GLfloat g4, GLfloat b4, GLfloat a4) {

  double length = sqrt(powf(x1 - x2, 2) + powf(y1 - y2, 2));
  double x = ((x2 - x1) / length) * width / 2;
  double y = ((y2 - y1) / length) * width / 2;

  R2D_DrawQuad(
    x1 - y, y1 + x, r1, g1, b1, a1,
    x1 + y, y1 - x, r2, g2, b2, a2,
    x2 + y, y2 - x, r3, g3, b3, a3,
    x2 - y, y2 + x, r4, g4, b4, a4
  );
};


/*
 * Draw a circle from triangles
 */
void R2D_DrawCircle(GLfloat x, GLfloat y, GLfloat radius, int sectors,
                    GLfloat r, GLfloat g, GLfloat b, GLfloat a) {

  double angle = 2 * M_PI / sectors;

  for (int i = 0; i < sectors; i++) {

    GLfloat x1 = x + radius * cos(i * angle);
    GLfloat y1 = y + radius * sin(i * angle);

    GLfloat x2 = x + radius * cos((i - 1) * angle);
    GLfloat y2 = y + radius * sin((i - 1) * angle);

    R2D_GL_DrawTriangle( x,  y, r, g, b, a,
                        x1, y1, r, g, b, a,
                        x2, y2, r, g, b, a);
  }
}
// sound.c

#include "ruby2d.h"


/*
 * Create a sound, given an audio file path
 */
R2D_Sound *R2D_CreateSound(const char *path) {
  R2D_Init();

  // Check if sound file exists
  if (!R2D_FileExists(path)) {
    R2D_Error("R2D_CreateSound", "Sound file `%s` not found", path);
    return NULL;
  }

  // Allocate the sound structure
  R2D_Sound *snd = (R2D_Sound *) malloc(sizeof(R2D_Sound));
  if (!snd) {
    R2D_Error("R2D_CreateSound", "Out of memory!");
    return NULL;
  }

  // Load the sound data from file
  snd->data = Mix_LoadWAV(path);
  if (!snd->data) {
    R2D_Error("Mix_LoadWAV", Mix_GetError());
    free(snd);
    return NULL;
  }

  // Initialize values
  snd->path = path;

  return snd;
}


/*
 * Play the sound
 */
void R2D_PlaySound(R2D_Sound *snd, bool loop) {
  if (!snd) return;

  // If looping, set to -1 times; else 0
  int times = loop ? -1 : 0;

  // times: 0 == once, -1 == forever
  snd->channel = Mix_PlayChannel(-1, snd->data, times);
}

/*
 * Stop the sound
 */
void R2D_StopSound(R2D_Sound *snd) {
  if (!snd) return;

  Mix_HaltChannel(snd->channel);
}


/*
 * Get the sound's length in seconds
 */
int R2D_GetSoundLength(R2D_Sound *snd) {
  float points = 0;
  float frames = 0;
  int frequency = 0;
  Uint16 format = 0;
  int channels = 0;

  // Populate the frequency, format and channel variables
  if (!Mix_QuerySpec(&frequency, &format, &channels)) return -1; // Querying audio details failed
  if (!snd) return -1;

  // points = bytes / samplesize
  points = (snd->data->alen / ((format & 0xFF) / 8));

  // frames = sample points / channels
  frames = (points / channels);

  // frames / frequency is seconds of audio
  return ceil(frames / frequency);
}


/*
 * Get the sound's volume
 */
int R2D_GetSoundVolume(R2D_Sound *snd) {
  if (!snd) return -1;
  return ceil(Mix_VolumeChunk(snd->data, -1) * (100.0 / MIX_MAX_VOLUME));
}


/*
 * Set the sound's volume a given percentage
 */
void R2D_SetSoundVolume(R2D_Sound *snd, int volume) {
  if (!snd) return;
  // Set volume to be a percentage of the maximum mix volume
  Mix_VolumeChunk(snd->data, (volume / 100.0) * MIX_MAX_VOLUME);
}


/*
 * Get the sound mixer volume
 */
int R2D_GetSoundMixVolume() {
  return ceil(Mix_Volume(-1, -1) * (100.0 / MIX_MAX_VOLUME));
}


/*
 * Set the sound mixer volume a given percentage
 */
void R2D_SetSoundMixVolume(int volume) {
  // This sets the volume value across all channels
  // Set volume to be a percentage of the maximum mix volume
  Mix_Volume(-1, (volume / 100.0) * MIX_MAX_VOLUME);
}


/*
 * Free the sound
 */
void R2D_FreeSound(R2D_Sound *snd) {
  if (!snd) return;
  Mix_FreeChunk(snd->data);
  free(snd);
}
// text.c

#include "ruby2d.h"


/*
 * Create a SDL_Surface that contains the pixel data to render text, given a font and message
 */
SDL_Surface *R2D_TextCreateSurface(TTF_Font *font, const char *message) {
  // `msg` cannot be an empty string or NULL for TTF_SizeText
  if (message == NULL || strlen(message) == 0) message = " ";

  SDL_Color color = {255, 255, 255};
  SDL_Surface *surface = TTF_RenderUTF8_Blended(font, message, color);
  if (!surface)
  {
    R2D_Error("TTF_RenderUTF8_Blended", TTF_GetError());
    return NULL;
  }

  // Re-pack surface for OpenGL
  // See: https://discourse.libsdl.org/t/sdl-ttf-2-0-18-surface-to-opengl-texture-not-consistent-with-ttf-2-0-15
  Sint32 i;
  Uint32 len = surface->w * surface->format->BytesPerPixel;
  Uint8 *src = surface->pixels;
  Uint8 *dst = surface->pixels;
  for (i = 0; i < surface->h; i++) {
    SDL_memmove(dst, src, len);
    dst += len;
    src += surface->pitch;
  }
  surface->pitch = len;

  return surface;
}
// window.c

#include "ruby2d.h"


/*
 * Create a window
 */
R2D_Window *R2D_CreateWindow(const char *title, int width, int height,
                             R2D_Update update, R2D_Render render, int flags) {

  R2D_Init();

  SDL_DisplayMode dm;
  SDL_GetCurrentDisplayMode(0, &dm);
  R2D_Log(R2D_INFO, "Current display mode is %dx%dpx @ %dhz", dm.w, dm.h, dm.refresh_rate);

  width  = width  == R2D_DISPLAY_WIDTH  ? dm.w : width;
  height = height == R2D_DISPLAY_HEIGHT ? dm.h : height;

  // Allocate window and set default values
  R2D_Window *window      = (R2D_Window *) malloc(sizeof(R2D_Window));
  window->sdl             = NULL;
  window->glcontext       = NULL;
  window->title           = title;
  window->width           = width;
  window->height          = height;
  window->orig_width      = width;
  window->orig_height     = height;
  window->viewport.width  = width;
  window->viewport.height = height;
  window->viewport.mode   = R2D_SCALE;
  window->update          = update;
  window->render          = render;
  window->flags           = flags;
  window->on_key          = NULL;
  window->on_mouse        = NULL;
  window->on_controller   = NULL;
  window->vsync           = true;
  window->fps_cap         = 60;
  window->background.r    = 0.0;
  window->background.g    = 0.0;
  window->background.b    = 0.0;
  window->background.a    = 1.0;
  window->icon            = NULL;
  window->close           = true;

  // Return the window structure
  return window;
}


const Uint8 *key_state;

Uint32 frames;
Uint32 frames_last_sec;
Uint32 start_ms;
Uint32 next_second_ms;
Uint32 begin_ms;
Uint32 end_ms;      // Time at end of loop
Uint32 elapsed_ms;  // Total elapsed time
Uint32 loop_ms;     // Elapsed time of current loop
int delay_ms;       // Amount of delay to achieve desired frame rate
double decay_rate;
double fps;

R2D_Window *window;





void main_loop() {

  // Clear Frame /////////////////////////////////////////////////////////////

  R2D_GL_Clear(window->background);

  // Set FPS /////////////////////////////////////////////////////////////////

  frames++;
  frames_last_sec++;
  end_ms = SDL_GetTicks();
  elapsed_ms = end_ms - start_ms;

  // Calculate the frame rate using an exponential moving average
  if (next_second_ms < end_ms) {
    fps = decay_rate * fps + (1.0 - decay_rate) * frames_last_sec;
    frames_last_sec = 0;
    next_second_ms = SDL_GetTicks() + 1000;
  }

  loop_ms = end_ms - begin_ms;
  delay_ms = (1000 / window->fps_cap) - loop_ms;

  if (delay_ms < 0) delay_ms = 0;

  // Note: `loop_ms + delay_ms` should equal `1000 / fps_cap`

  #ifndef __EMSCRIPTEN__
    SDL_Delay(delay_ms);
  #endif

  begin_ms = SDL_GetTicks();

  // Handle Input and Window Events //////////////////////////////////////////

  int mx, my;  // mouse x, y coordinates

  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    switch (e.type) {

      case SDL_KEYDOWN:
        if (window->on_key && e.key.repeat == 0) {
          R2D_Event event = {
            .type = R2D_KEY_DOWN, .key = SDL_GetScancodeName(e.key.keysym.scancode)
          };
          window->on_key(event);
        }
        break;

      case SDL_KEYUP:
        if (window->on_key) {
          R2D_Event event = {
            .type = R2D_KEY_UP, .key = SDL_GetScancodeName(e.key.keysym.scancode)
          };
          window->on_key(event);
        }
        break;

      case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
        if (window->on_mouse) {
          R2D_GetMouseOnViewport(window, e.button.x, e.button.y, &mx, &my);
          R2D_Event event = {
            .button = e.button.button, .x = mx, .y = my
          };
          event.type = e.type == SDL_MOUSEBUTTONDOWN ? R2D_MOUSE_DOWN : R2D_MOUSE_UP;
          event.dblclick = e.button.clicks == 2 ? true : false;
          window->on_mouse(event);
        }
        break;

      case SDL_MOUSEWHEEL:
        if (window->on_mouse) {
          R2D_Event event = {
            .type = R2D_MOUSE_SCROLL, .direction = e.wheel.direction,
            .delta_x = e.wheel.x, .delta_y = -e.wheel.y
          };
          window->on_mouse(event);
        }
        break;

      case SDL_MOUSEMOTION:
        if (window->on_mouse) {
          R2D_GetMouseOnViewport(window, e.motion.x, e.motion.y, &mx, &my);
          R2D_Event event = {
            .type = R2D_MOUSE_MOVE,
            .x = mx, .y = my, .delta_x = e.motion.xrel, .delta_y = e.motion.yrel
          };
          window->on_mouse(event);
        }
        break;

      case SDL_CONTROLLERAXISMOTION:
        if (window->on_controller) {
          R2D_Event event = {
            .which = e.caxis.which, .type = R2D_AXIS,
            .axis = e.caxis.axis, .value = e.caxis.value
          };
          window->on_controller(event);
        }
        break;

      case SDL_JOYAXISMOTION:
        if (window->on_controller && !R2D_IsController(e.jbutton.which)) {
          R2D_Event event = {
            .which = e.jaxis.which, .type = R2D_AXIS,
            .axis = e.jaxis.axis, .value = e.jaxis.value
          };
          window->on_controller(event);
        }
        break;

      case SDL_CONTROLLERBUTTONDOWN: case SDL_CONTROLLERBUTTONUP:
        if (window->on_controller) {
          R2D_Event event = {
            .which = e.cbutton.which, .button = e.cbutton.button
          };
          event.type = e.type == SDL_CONTROLLERBUTTONDOWN ? R2D_BUTTON_DOWN : R2D_BUTTON_UP;
          window->on_controller(event);
        }
        break;

      case SDL_JOYBUTTONDOWN: case SDL_JOYBUTTONUP:
        if (window->on_controller && !R2D_IsController(e.jbutton.which)) {
          R2D_Event event = {
            .which = e.jbutton.which, .button = e.jbutton.button
          };
          event.type = e.type == SDL_JOYBUTTONDOWN ? R2D_BUTTON_DOWN : R2D_BUTTON_UP;
          window->on_controller(event);
        }
        break;

      case SDL_JOYDEVICEADDED:
        R2D_Log(R2D_INFO, "Controller connected (%i total)", SDL_NumJoysticks());
        R2D_OpenControllers();
        break;

      case SDL_JOYDEVICEREMOVED:
        if (R2D_IsController(e.jdevice.which)) {
          R2D_Log(R2D_INFO, "Controller #%i: %s removed (%i remaining)", e.jdevice.which, SDL_GameControllerName(SDL_GameControllerFromInstanceID(e.jdevice.which)), SDL_NumJoysticks());
          SDL_GameControllerClose(SDL_GameControllerFromInstanceID(e.jdevice.which));
        } else {
          R2D_Log(R2D_INFO, "Controller #%i: %s removed (%i remaining)", e.jdevice.which, SDL_JoystickName(SDL_JoystickFromInstanceID(e.jdevice.which)), SDL_NumJoysticks());
          SDL_JoystickClose(SDL_JoystickFromInstanceID(e.jdevice.which));
        }
        break;

      case SDL_WINDOWEVENT:
        switch (e.window.event) {
          case SDL_WINDOWEVENT_RESIZED:
            // Store new window size, set viewport
            window->width  = e.window.data1;
            window->height = e.window.data2;
            R2D_GL_SetViewport(window);
            break;
        }
        break;

      case SDL_QUIT:
        R2D_Close(window);
        break;
    }
  }

  // Detect keys held down
  int num_keys;
  key_state = SDL_GetKeyboardState(&num_keys);

  for (int i = 0; i < num_keys; i++) {
    if (window->on_key) {
      if (key_state[i] == 1) {
        R2D_Event event = {
          .type = R2D_KEY_HELD, .key = SDL_GetScancodeName(i)
        };
        window->on_key(event);
      }
    }
  }

  // Get and store mouse position relative to the viewport
  int wx, wy;  // mouse x, y coordinates relative to the window
  SDL_GetMouseState(&wx, &wy);
  R2D_GetMouseOnViewport(window, wx, wy, &window->mouse.x, &window->mouse.y);

  // Update Window State /////////////////////////////////////////////////////

  // Store new values in the window
  window->frames     = frames;
  window->elapsed_ms = elapsed_ms;
  window->loop_ms    = loop_ms;
  window->delay_ms   = delay_ms;
  window->fps        = fps;

  // Call update and render callbacks
  if (window->update) window->update();
  if (window->render) window->render();

  // Draw Frame //////////////////////////////////////////////////////////////

  // Render and flush all OpenGL buffers
  R2D_GL_FlushBuffers();

  // Swap buffers to display drawn contents in the window
  SDL_GL_SwapWindow(window->sdl);
}












/*
 * Show the window
 */
int R2D_Show(R2D_Window *win) {

  window = win;

  if (!window) {
    R2D_Error("R2D_Show", "Window cannot be shown (because it's NULL)");
    return 1;
  }

  // Create SDL window
  window->sdl = SDL_CreateWindow(
    window->title,                                   // title
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,  // window position
    window->width, window->height,                   // window size
    SDL_WINDOW_OPENGL | window->flags                // flags
  );

  if (!window->sdl) R2D_Error("SDL_CreateWindow", SDL_GetError());
  if (window->icon) R2D_SetIcon(window, window->icon);

  // The window created by SDL might not actually be the requested size.
  // If it's not the same, retrieve and store the actual window size.
  int actual_width, actual_height;
  SDL_GetWindowSize(window->sdl, &actual_width, &actual_height);

  if ((window->width != actual_width) || (window->height != actual_height)) {
    R2D_Log(R2D_INFO,
      "Scaling window to %ix%i (requested size was %ix%i)",
      actual_width, actual_height, window->width, window->height
    );
    window->width  = actual_width;
    window->height = actual_height;
    window->orig_width  = actual_width;
    window->orig_height = actual_height;
  }

  // Set Up OpenGL /////////////////////////////////////////////////////////////

  R2D_GL_Init(window);

  // SDL 2.0.10 and macOS 10.15 fix ////////////////////////////////////////////

  #if MACOS
    SDL_SetWindowSize(window->sdl, window->width, window->height);
  #endif

  // Set Main Loop Data ////////////////////////////////////////////////////////


  frames = 0;           // Total frames since start
  frames_last_sec = 0;  // Frames in the last second
  start_ms = SDL_GetTicks();  // Elapsed time since start
  next_second_ms = SDL_GetTicks(); // The last time plus a second
  begin_ms = start_ms;  // Time at beginning of loop
  decay_rate = 0.5;  // Determines how fast an average decays over time
  fps = window->fps_cap;   // Moving average of actual FPS, initial value a guess

  // Enable VSync
  if (window->vsync) {
    if (!SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1")) {
      R2D_Log(R2D_WARN, "VSync cannot be enabled");
    }
  }

  window->close = false;

  // Main Loop /////////////////////////////////////////////////////////////////

  #ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop, 0, true);
  #else
    while (!window->close) main_loop();
  #endif

  return 0;
}



/*
 * Set the icon for the window
 */
void R2D_SetIcon(R2D_Window *window, const char *icon) {
  SDL_Surface *iconSurface = R2D_CreateImageSurface(icon);
  if (iconSurface) {
    window->icon = icon;
    SDL_SetWindowIcon(window->sdl, iconSurface);
    SDL_FreeSurface(iconSurface);
  } else {
    R2D_Log(R2D_WARN, "Could not set window icon");
  }
}


/*
 * Take a screenshot of the window
 */
void R2D_Screenshot(R2D_Window *window, const char *path) {

  #if GLES
    R2D_Error("R2D_Screenshot", "Not supported in OpenGL ES");
  #else
    // Create a surface the size of the window
    SDL_Surface *surface = SDL_CreateRGBSurface(
      SDL_SWSURFACE, window->width, window->height, 24,
      0x000000FF, 0x0000FF00, 0x00FF0000, 0
    );

    // Grab the pixels from the front buffer, save to surface
    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, window->width, window->height, GL_RGB, GL_UNSIGNED_BYTE, surface->pixels);

    // Flip image vertically

    void *temp_row = (void *)malloc(surface->pitch);
    if (!temp_row) {
      R2D_Error("R2D_Screenshot", "Out of memory!");
      SDL_FreeSurface(surface);
      return;
    }

    int height_div_2 = (int) (surface->h * 0.5);

    for (int index = 0; index < height_div_2; index++) {
      memcpy((Uint8 *)temp_row,(Uint8 *)(surface->pixels) + surface->pitch * index, surface->pitch);
      memcpy((Uint8 *)(surface->pixels) + surface->pitch * index, (Uint8 *)(surface->pixels) + surface->pitch * (surface->h - index-1), surface->pitch);
      memcpy((Uint8 *)(surface->pixels) + surface->pitch * (surface->h - index-1), temp_row, surface->pitch);
    }

    free(temp_row);

    // Save image to disk
    IMG_SavePNG(surface, path);
    SDL_FreeSurface(surface);
  #endif
}


/*
 * Close the window
 */
int R2D_Close(R2D_Window *window) {
  if (!window->close) {
    R2D_Log(R2D_INFO, "Closing window");
    window->close = true;
  }
  return 0;
}


/*
 * Free all resources
 */
int R2D_FreeWindow(R2D_Window *window) {
  R2D_Close(window);
  SDL_GL_DeleteContext(window->glcontext);
  SDL_DestroyWindow(window->sdl);
  free(window);
  return 0;
}
