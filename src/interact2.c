#include "../include/visky/interact2.h"



/*************************************************************************************************/
/*  Constants                                                                                    */
/*************************************************************************************************/

#define VKL_NEVER                        -1000000
#define VKL_MOUSE_CLICK_MAX_DELAY        .5
#define VKL_MOUSE_CLICK_MAX_SHIFT        10
#define VKL_MOUSE_DOUBLE_CLICK_MAX_DELAY .2
#define VKL_KEY_PRESS_DELAY              .05
#define VKL_PANZOOM_MOUSE_WHEEL_FACTOR   2
#define VKL_PANZOOM_MIN_ZOOM             1e-4
#define VKL_PANZOOM_MAX_ZOOM             1e+4


/*************************************************************************************************/
/*  Mouse                                                                                        */
/*************************************************************************************************/

VklMouseState vkl_mouse()
{
    VklMouseState mouse = {0};
    vkl_mouse_reset(&mouse);
    return mouse;
}



void vkl_mouse_reset(VklMouseState* mouse)
{
    ASSERT(mouse != NULL);
    memset(mouse, 0, sizeof(VklMouseState));
    // mouse->button = VKL_MOUSE_BUTTON_NONE;
    // glm_vec2_zero(mouse->cur_pos);
    // glm_vec2_zero(mouse->press_pos);
    // glm_vec2_zero(mouse->last_pos);
    // mouse->cur_state = VKL_MOUSE_STATE_INACTIVE;
    mouse->press_time = VKL_NEVER;
    mouse->click_time = VKL_NEVER;
}



static void _viewport_center(VklViewport viewport, vec2 pos)
{
    pos[0] = viewport.size_screen[0] + viewport.size_screen[2] / 2;
    pos[1] = viewport.size_screen[1] + viewport.size_screen[3] / 2;
}

void vkl_mouse_event(VklMouseState* mouse, VklCanvas* canvas, VklViewport viewport, VklEvent ev)
{
    double time = canvas->clock.elapsed;
    // _viewport_center(viewport, mouse->cur_pos);
    // vec2 pos = {0};
    // VklMouseButton button = VKL_MOUSE_BUTTON_NONE;

    // Update the last pos.
    glm_vec2_copy(mouse->cur_pos, mouse->last_pos);

    // Reset click events as soon as the next loop iteration after they were raised.
    if (mouse->cur_state == VKL_MOUSE_STATE_CLICK ||
        mouse->cur_state == VKL_MOUSE_STATE_DOUBLE_CLICK)
    {
        mouse->cur_state = VKL_MOUSE_STATE_INACTIVE;
        mouse->button = VKL_MOUSE_BUTTON_NONE;
    }

    // bool pressed = button != VKL_MOUSE_BUTTON_NONE;

    // Net distance in pixels since the last press event.
    vec2 shift;

    switch (ev.type)
    {

    case VKL_EVENT_MOUSE_BUTTON:

        // Press event.
        if (ev.u.b.type == VKL_MOUSE_PRESS && mouse->press_time == VKL_NEVER)
        {
            glm_vec2_copy(mouse->cur_pos, mouse->press_pos);
            mouse->press_time = time;
            mouse->button = ev.u.b.button;
        }

        // Release event.
        else if (ev.u.b.type == VKL_MOUSE_RELEASE)
        {
            // End drag.
            if (mouse->cur_state == VKL_MOUSE_STATE_DRAG)
            {
                log_debug("end drag event");
                mouse->cur_state = VKL_MOUSE_STATE_INACTIVE;
                mouse->button = VKL_MOUSE_BUTTON_NONE;
            }

            // Double click event.
            else if (time - mouse->click_time < VKL_MOUSE_DOUBLE_CLICK_MAX_DELAY)
            {
                // NOTE: when releasing, current button is NONE so we must use the previously set
                // button in mouse->button.
                log_debug("double click event on button %d", mouse->button);
                mouse->cur_state = VKL_MOUSE_STATE_DOUBLE_CLICK;
                mouse->click_time = time;
            }

            // Click event.
            else if (
                time - mouse->press_time < VKL_MOUSE_CLICK_MAX_DELAY &&
                mouse->shift_length < VKL_MOUSE_CLICK_MAX_SHIFT)
            {
                log_debug("click event on button %d", mouse->button);
                mouse->cur_state = VKL_MOUSE_STATE_CLICK;
                mouse->click_time = time;
            }

            else
            {
                // Reset the mouse button state.
                mouse->button = VKL_MOUSE_BUTTON_NONE;
            }
            mouse->press_time = VKL_NEVER;
        }
        mouse->shift_length = 0;
        // mouse->button = ev.u.b.button;

        // log_debug("mouse button %d", mouse->button);
        break;


    case VKL_EVENT_MOUSE_MOVE:
        glm_vec2_copy(ev.u.m.pos, mouse->cur_pos);

        // Update the distance since the last press position.
        if (mouse->button != VKL_MOUSE_BUTTON_NONE)
        {
            glm_vec2_sub(mouse->cur_pos, mouse->press_pos, shift);
            mouse->shift_length = glm_vec2_norm(shift);
        }

        // Mouse move event only if the shift length is larger than the click area.
        if (mouse->shift_length > VKL_MOUSE_CLICK_MAX_SHIFT)
        {
            // Mouse move.
            if (mouse->cur_state == VKL_MOUSE_STATE_INACTIVE &&
                mouse->button != VKL_MOUSE_BUTTON_NONE)
            {
                log_debug("drag event on button %d", mouse->button);
                mouse->cur_state = VKL_MOUSE_STATE_DRAG;
            }
        }
        // log_debug("mouse mouse %.1fx%.1f", mouse->cur_pos[0], mouse->cur_pos[1]);
        break;


    case VKL_EVENT_MOUSE_WHEEL:
        glm_vec2_copy(ev.u.w.dir, mouse->wheel_delta);
        log_debug("mouse wheel %.1f", mouse->wheel_delta[1]);
        break;

    default:
        break;
    }
}



/*************************************************************************************************/
/*  Keyboard                                                                                     */
/*************************************************************************************************/

VklKeyboardState vkl_keyboard()
{
    VklKeyboardState keyboard = {0};
    vkl_keyboard_reset(&keyboard);
    return keyboard;
}



void vkl_keyboard_reset(VklKeyboardState* keyboard)
{
    ASSERT(keyboard != NULL);
    memset(keyboard, 0, sizeof(VklKeyboardState));
    // keyboard->key_code = VKL_KEY_NONE;
    // keyboard->modifiers = 0;
    keyboard->press_time = VKL_NEVER;
}



static bool _is_key_modifier(VklKeyCode key)
{
    return (
        key == VKL_KEY_LEFT_SHIFT || key == VKL_KEY_RIGHT_SHIFT || key == VKL_KEY_LEFT_CONTROL ||
        key == VKL_KEY_RIGHT_CONTROL || key == VKL_KEY_LEFT_ALT || key == VKL_KEY_RIGHT_ALT ||
        key == VKL_KEY_LEFT_SUPER || key == VKL_KEY_RIGHT_SUPER);
}

void vkl_keyboard_event(
    VklKeyboardState* keyboard, VklCanvas* canvas, VklViewport viewport, VklEvent ev)
{
    double time = canvas->clock.elapsed;
    VklKeyCode key = ev.u.k.key_code;

    if (time - keyboard->press_time < VKL_KEY_PRESS_DELAY)
        return;
    if (ev.u.k.type == VKL_KEY_PRESS)
    {
        log_debug("key pressed %d mods %d", key, ev.u.k.modifiers);
        keyboard->key_code = key;
        keyboard->modifiers = ev.u.k.modifiers;
        keyboard->press_time = time;
        if (keyboard->cur_state == VKL_KEYBOARD_STATE_INACTIVE)
            keyboard->cur_state = VKL_KEYBOARD_STATE_ACTIVE;
    }
    else
    {
        if (keyboard->cur_state == VKL_KEYBOARD_STATE_ACTIVE)
            keyboard->cur_state = VKL_KEYBOARD_STATE_INACTIVE;
    }
}



/*************************************************************************************************/
/*  Mouse normalization utils                                                                    */
/*************************************************************************************************/

static inline void _mouse_normalize(vec2 size, vec2 center, vec2 pos)
{
    pos[0] -= center[0];
    pos[1] -= center[1];

    pos[0] *= +2 / size[0];
    pos[1] *= -2 / size[1];
}

static inline void _mouse_normalize_viewport(VklViewport viewport, vec2 pos)
{
    if (viewport.size_screen[0] == 0)
    {
        log_trace("skipping normalization as the canvas size has not been set yet");
    }
    else
    {
        pos[0] *= (viewport.size_framebuffer[0] / viewport.size_screen[0]);
        pos[1] *= (viewport.size_framebuffer[1] / viewport.size_screen[1]);
    }

    // viewport center in pixels
    vec2 center = {
        viewport.viewport.x + viewport.viewport.width / 2,
        viewport.viewport.y + viewport.viewport.height / 2};
    vec2 size = {viewport.viewport.width, viewport.viewport.height};
    _mouse_normalize(size, center, pos);
}

static inline void _mouse_move_delta(VklMouseState* mouse, VklViewport viewport, vec2 delta)
{
    vec2 delta_ = {
        mouse->cur_pos[0] - mouse->last_pos[0],
        mouse->cur_pos[1] - mouse->last_pos[1],
    };
    delta_[0] = +2 * delta_[0] / viewport.size_screen[0];
    delta_[1] = -2 * delta_[1] / viewport.size_screen[1];
    delta_[0] *= (viewport.size_framebuffer[0] / viewport.size_screen[0]);
    delta_[1] *= (viewport.size_framebuffer[1] / viewport.size_screen[1]);
    glm_vec2_copy(delta_, delta);
}

// in local normalized coordinates in the viewport
static inline void _mouse_cur_pos(VklMouseState* mouse, VklViewport viewport, vec2 pos)
{
    // Mouse position in window coordinates.
    glm_vec2_copy(mouse->cur_pos, pos);
    // Mouse position in viewport normalized coordinates.
    _mouse_normalize_viewport(viewport, pos);
}

// in local normalized coordinates in the viewport
static inline void _mouse_press_pos(VklMouseState* mouse, VklViewport viewport, vec2 pos)
{
    // Mouse position in window coordinates.
    glm_vec2_copy(mouse->press_pos, pos);
    // Mouse position in viewport normalized coordinates.
    _mouse_normalize_viewport(viewport, pos);
}



/*************************************************************************************************/
/*  Panzoom                                                                                      */
/*************************************************************************************************/

static VklPanzoom _panzoom()
{
    VklPanzoom p = {0};
    p.camera_pos[2] = +2;
    p.zoom[0] = 1;
    p.zoom[1] = 1;
    return p;
}

static void _panzoom_callback(
    VklInteract* interact, VklViewport viewport, VklMouseState* mouse, VklKeyboardState* keyboard)
{
    ASSERT(interact != NULL);
    ASSERT(interact->type == VKL_INTERACT_PANZOOM);
    VklCanvas* canvas = interact->canvas;
    VklPanzoom* panzoom = &interact->u.p;

    // TODO
    float aspect_ratio = 1;

    float wheel_factor = VKL_PANZOOM_MOUSE_WHEEL_FACTOR;

    // Window size.
    uvec2 size_w, size_b;
    vkl_canvas_size(canvas, VKL_CANVAS_SIZE_SCREEN, size_w);
    vkl_canvas_size(canvas, VKL_CANVAS_SIZE_FRAMEBUFFER, size_b);

#if OS_MACOS
    // HACK: touchpad wheel too sensitive on macOS
    wheel_factor *= -.1;
#endif

    // Pan.
    if (mouse->cur_state == VKL_MOUSE_STATE_DRAG && mouse->button == VKL_MOUSE_BUTTON_LEFT)
    {
        // TODO
        // Restrict the panzoom updates to cases when the mouse press position was in the panel.
        // if (vkl_panel_from_mouse(scene, mouse->press_pos) != panel)
        //     return;
        // panel->status = VKL_PANEL_STATUS_ACTIVE;

        vec2 delta = {0};
        _mouse_move_delta(mouse, viewport, delta);

        if (aspect_ratio == 1)
            delta[0] *= size_b[0] / size_b[1];

        panzoom->camera_pos[0] -= delta[0] / panzoom->zoom[0];
        panzoom->camera_pos[1] -= delta[1] / panzoom->zoom[1];
    } // end pan

    // Zoom.
    if ((mouse->cur_state == VKL_MOUSE_STATE_DRAG && mouse->button == VKL_MOUSE_BUTTON_RIGHT) ||
        mouse->cur_state == VKL_MOUSE_STATE_WHEEL)
    {
        vec2 pan, delta, zoom_old, zoom_new, center;

        // Right drag.
        if (mouse->cur_state == VKL_MOUSE_STATE_DRAG && mouse->button == VKL_MOUSE_BUTTON_RIGHT)
        {

            // TODO
            // Restrict the panzoom updates to cases when the mouse press position was in the
            // panel.
            // if (vkl_panel_from_mouse(scene, mouse->press_pos) != panel)
            //     return;
            // panel->status = VKL_PANEL_STATUS_ACTIVE;

            // Get the center position: mouse press position.
            _mouse_press_pos(mouse, viewport, center);

            // Get the mouse move delta.
            _mouse_move_delta(mouse, viewport, delta);
            // Transform back the delta in pixels.
            delta[0] *= size_b[0] / 2;
            delta[1] *= size_b[1] / 2;
            delta[0] *= .0025;
            delta[1] *= .0025;
        }
        // Mouse wheel.
        else
        {

            // TODO
            // Restrict the panzoom updates to cases when the mouse press position was in the
            // panel.
            // if (vkl_panel_from_mouse(scene, mouse->cur_pos) != panel)
            //     return;
            // panel->status = VKL_PANEL_STATUS_ACTIVE;

            _mouse_cur_pos(mouse, viewport, center);
            glm_vec2_copy(mouse->wheel_delta, delta);
            delta[0] *= wheel_factor;
            delta[1] *= wheel_factor;
        }

        // Fixed aspect ratio.
        if (aspect_ratio == 1)
        {
            delta[0] = delta[1] = copysignf(1.0, delta[0] + delta[1]) *
                                  sqrt(delta[0] * delta[0] + delta[1] * delta[1]);

            center[0] *= size_b[0] / size_b[1];
        }

        // Update the zoom.
        glm_vec2_copy(panzoom->zoom, zoom_old);
        glm_vec2_copy(panzoom->zoom, zoom_new);
        glm_vec2_mul(zoom_new, (vec2){delta[0] + 1, delta[1] + 1}, zoom_new);

        // Clip zoom x.
        double zx = zoom_new[0];
        if (zx <= VKL_PANZOOM_MIN_ZOOM || zx >= VKL_PANZOOM_MAX_ZOOM)
        {
            zoom_new[0] = CLIP(zx, VKL_PANZOOM_MIN_ZOOM, VKL_PANZOOM_MAX_ZOOM);
            panzoom->lim_reached[0] = true;
        }
        else
        {
        }
        // Clip zoom y.
        double zy = zoom_new[1];
        if (zy <= VKL_PANZOOM_MIN_ZOOM || zy >= VKL_PANZOOM_MAX_ZOOM)
        {
            zoom_new[1] = CLIP(zy, VKL_PANZOOM_MIN_ZOOM, VKL_PANZOOM_MAX_ZOOM);
            panzoom->lim_reached[1] = true;
        }
        else
        {
        }

        // Update zoom.
        if (!panzoom->lim_reached[0])
            panzoom->zoom[0] = zoom_new[0];
        if (!panzoom->lim_reached[1])
            panzoom->zoom[1] = zoom_new[1];

        // Update pan.
        pan[0] = -center[0] * (1.0f / zoom_old[0] - 1.0f / zoom_new[0]) * zoom_new[0];
        pan[1] = -center[1] * (1.0f / zoom_old[1] - 1.0f / zoom_new[1]) * zoom_new[1];

        if (!panzoom->lim_reached[0])
            panzoom->camera_pos[0] -= pan[0] / panzoom->zoom[0];
        if (!panzoom->lim_reached[1])
            panzoom->camera_pos[1] -= pan[1] / panzoom->zoom[1];
    } // end zoom

    // Reset on double-click.
    if (mouse->cur_state == VKL_MOUSE_STATE_DOUBLE_CLICK)
    {

        // TODO
        // // Restrict the panzoom updates to cases when the mouse press position was in the panel.
        // if (vkl_panel_from_mouse(scene, mouse->cur_pos) != panel)
        //     return;
        // panel->status = VKL_PANEL_STATUS_RESET;

        panzoom->camera_pos[0] = 0;
        panzoom->camera_pos[1] = 0;
        panzoom->zoom[0] = 1;
        panzoom->zoom[1] = 1;
    }

    if (mouse->cur_state == VKL_MOUSE_STATE_INACTIVE)
    {
        // panel->status = VKL_PANEL_STATUS_NONE;
    }
}



/*************************************************************************************************/
/*  Arcball                                                                                      */
/*************************************************************************************************/

// adapted from https://github.com/Twinklebear/arcball-cpp/blob/master/arcball_panel.cpp
/*
static void _reset_arcball(VklArcball* arcball)
{

    vec3 eye, center, up, dir, x_axis, y_axis, z_axis;
    glm_vec3_copy(arcball->eye_init, eye);
    glm_vec3_copy((vec3){0, 0, 0}, center);
    glm_vec3_copy((vec3){0, +1, 0}, up);

    glm_vec3_sub(center, eye, dir);
    glm_vec3_copy(dir, z_axis);
    glm_vec3_normalize(z_axis);
    glm_vec3_normalize(up);

    glm_vec3_cross(z_axis, up, x_axis);
    glm_vec3_normalize(x_axis);

    glm_vec3_cross(x_axis, z_axis, y_axis);
    glm_vec3_normalize(y_axis);

    glm_vec3_cross(z_axis, y_axis, x_axis);
    glm_vec3_normalize(x_axis);

    glm_translate_make(arcball->center_translation, center);
    glm_mat4_inv(arcball->center_translation, arcball->center_translation);

    glm_translate_make(arcball->translation, (vec3){0, 0, -glm_vec3_norm(dir)});

    mat3 m;
    glm_vec3_copy((vec3){-x_axis[0], -x_axis[1], -x_axis[2]}, m[0]);
    glm_vec3_copy((vec3){+y_axis[0], +y_axis[1], +y_axis[2]}, m[1]);
    glm_vec3_copy((vec3){+z_axis[0], +z_axis[1], +z_axis[2]}, m[2]);

    glm_mat3_transpose(m);
    glm_mat3_quat(m, arcball->rotation);
    glm_quat_normalize(arcball->rotation);

    glm_mat4_identity(arcball->mat_user);
}

VklArcball* vkl_arcball_init(VklMVPMatrix which_matrix)
{
    // Allocate the VklArcball instance on the heap.
    VklArcball* arcball = (VklArcball*)calloc(1, sizeof(VklArcball));
    if (which_matrix == VKL_MVP_VIEW)
    {
        arcball->eye_init[2] = 2;
    }
    arcball->which_matrix = which_matrix;

    _reset_arcball(arcball);

    arcball->mvp = vkl_create_mvp();

    if (which_matrix == VKL_MVP_MODEL)
    {
        // If the arcball modifies the model matrix, the view matrix should be set to the standard
        // 3D view matrix.
        vkl_mvp_set_view_3D(
            (vec3)VKL_DEFAULT_CAMERA_POS, (vec3)VKL_DEFAULT_CAMERA_CENTER, &arcball->mvp);
    }

    return arcball;
}

void vkl_screen_to_arcball(vec2 p, versor q)
{
    float dist = glm_vec2_dot(p, p);
    // If we're on/in the sphere return the point on it
    if (dist <= 1.f)
    {
        glm_vec4_copy((vec4){p[0], -p[1], sqrt(1 - dist), 0}, q);
    }
    else
    {
        // otherwise we project the point onto the sphere
        glm_vec2_normalize(p);
        glm_vec4_copy((vec4){p[0], -p[1], 0, 0}, q);
    }
}

void vkl_arcball_update(VklPanel* panel, VklArcball* arcball, VklViewportType viewport_type)
{
    VklScene* scene = panel->scene;
    VklMouse* mouse = scene->canvas->event_controller->mouse;
    VklViewport viewport = vkl_get_viewport(panel, viewport_type);

    // Rotate.
    if (mouse->cur_state == VKL_MOUSE_STATE_DRAG && mouse->button == VKL_MOUSE_BUTTON_LEFT)
    {
        vec2 cur_pos, last_pos;

        // Restrict the panzoom updates to cases when the mouse press position was in the panel.
        if (vkl_panel_from_mouse(scene, mouse->press_pos) != panel)
            return;
        panel->status = VKL_PANEL_STATUS_ACTIVE;

        _mouse_cur_pos(mouse, viewport, cur_pos);
        _mouse_last_pos(mouse, viewport, last_pos);

        // NOTE: need to invert the mouse normalized coordinates if the standard 3D view matrix is
        // also applied.
        if (arcball->which_matrix == VKL_MVP_MODEL)
        {
            cur_pos[0] *= -1;
            cur_pos[1] *= -1;
            last_pos[0] *= -1;
            last_pos[1] *= -1;
        }

        versor mouse_cur_ball = {0}, mouse_prev_ball = {0};
        vkl_screen_to_arcball(cur_pos, mouse_cur_ball);
        vkl_screen_to_arcball(last_pos, mouse_prev_ball);

        glm_quat_mul(mouse_prev_ball, arcball->rotation, arcball->rotation);
        glm_quat_mul(mouse_cur_ball, arcball->rotation, arcball->rotation);
    }

    // Zoom.
    if (mouse->cur_state == VKL_MOUSE_STATE_WHEEL)
    {

        // Restrict the panzoom updates to cases when the mouse press position was in the panel.
        if (vkl_panel_from_mouse(scene, mouse->cur_pos) != panel)
            return;
        panel->status = VKL_PANEL_STATUS_ACTIVE;

        vec3 motion = {0, 0, -2 * mouse->wheel_delta[1]};
        mat4 tr;
        glm_translate_make(tr, motion);
        glm_mat4_mul(tr, arcball->translation, arcball->translation);
        // Zoom bound.
        // float zoom_max = 1;
        // if (arcball->translation[3][2] > -zoom_max)
        //     arcball->translation[3][2] = -zoom_max;
    }

    // Reset with double-click.
    if (mouse->cur_state == VKL_MOUSE_STATE_DOUBLE_CLICK)
    {
        _reset_arcball(arcball);
        panel->status = VKL_PANEL_STATUS_RESET;
    }

    // Compute the View matrix.
    mat4 arcball_mat;
    glm_quat_mat4(arcball->rotation, arcball_mat);
    glm_mat4_mul(arcball_mat, arcball->center_translation, arcball_mat);
    glm_mat4_mul(arcball->translation, arcball_mat, arcball_mat);

    // Pan.
    if (mouse->cur_state == VKL_MOUSE_STATE_DRAG && mouse->button == VKL_MOUSE_BUTTON_RIGHT)
    {
        if (vkl_panel_from_mouse(scene, mouse->press_pos) != panel)
            return;
        panel->status = VKL_PANEL_STATUS_ACTIVE;

        // float zoom_amount = 1;//abs(arcball->translation[3][2]);
        vec2 delta;
        _mouse_move_delta(mouse, viewport, delta);

        // NOTE: need to invert the mouse normalized coordinates if the standard 3D view matrix is
        // also applied.
        if (arcball->which_matrix == VKL_MVP_MODEL)
        {
            delta[0] *= -1;
            delta[1] *= -1;
        }

        vec4 motion = {
            VKL_ARCBALL_PAN_FACTOR * delta[0], VKL_ARCBALL_PAN_FACTOR * -delta[1], 0, 0};
        // Find the panning amount in the world space
        mat4 inv_panel;
        glm_mat4_inv(arcball_mat, inv_panel);
        glm_mat4_mulv(inv_panel, motion, motion);
        mat4 tr;
        glm_translate_make(tr, motion);
        glm_mat4_mul(tr, arcball->center_translation, arcball->center_translation);

        // Update the view matrix.
        glm_quat_mat4(arcball->rotation, arcball_mat);
        glm_mat4_mul(arcball_mat, arcball->center_translation, arcball_mat);
        glm_mat4_mul(arcball->translation, arcball_mat, arcball_mat);
    }

    // Make a copy of the transformation matrix, if other controllers or the user want to modify
    // the model/view matrix.
    glm_mat4_copy(arcball_mat, arcball->mat_arcball);

    // Take the user matrix into account.
    glm_mat4_mul(arcball->mat_arcball, arcball->mat_user, arcball->mat_arcball);

    // Update the panels with the MVP matrices.
    if (arcball->which_matrix == VKL_MVP_MODEL)
    {
        vkl_mvp_set_model(arcball->mat_arcball, &arcball->mvp);
    }
    else if (arcball->which_matrix == VKL_MVP_VIEW)
    {
        vkl_mvp_set_view(arcball->mat_arcball, &arcball->mvp);
    }
    else
    {
        log_error("the matrix passed to the arcball controller should be either the model or the "
                  "view matrix");
    }
    vkl_mvp_set_proj_3D(panel, viewport_type, &arcball->mvp);
    vkl_mvp_upload(panel, viewport_type, &arcball->mvp);
    vkl_mvp_finalize(scene);

    if (mouse->cur_state == VKL_MOUSE_STATE_INACTIVE)
    {
        panel->status = VKL_PANEL_STATUS_NONE;
    }
}
*/

static VklArcball _arcball()
{
    VklArcball a = {0};
    // TODO
    return a;
}

static void _arcball_callback(
    VklInteract* interact, VklViewport viewport, VklMouseState* mouse, VklKeyboardState* keyboard)
{
    ASSERT(interact != NULL);
    ASSERT(interact->type == VKL_INTERACT_ARCBALL);
    // TODO
}



/*************************************************************************************************/
/*  FPS camera                                                                                   */
/*************************************************************************************************/
/*
static void _reset_camera(VklCamera* camera)
{
    glm_vec3_copy((vec3)VKL_DEFAULT_CAMERA_POS, camera->eye);
    glm_vec3_copy(camera->eye, camera->target);
    glm_vec3_copy((vec3){0, 0, -1}, camera->forward);
    glm_vec3_copy((vec3)VKL_DEFAULT_CAMERA_UP, camera->up);

    camera->mvp = vkl_create_mvp();
}

VklCamera* vkl_camera_init()
{
    VklCamera* camera = (VklCamera*)calloc(1, sizeof(VklCamera));
    _reset_camera(camera);
    return camera;
}

void vkl_camera_update(VklPanel* panel, VklCamera* camera, VklViewportType viewport_type)
{
    VklScene* scene = panel->scene;
    VklControllerType controller_type = panel->controller_type;
    VklKeyboard* keyboard = scene->canvas->event_controller->keyboard;
    VklMouse* mouse = scene->canvas->event_controller->mouse;

    // double t = scene->canvas->local_time;
    const float increment = .35;
    const float max_pitch = .99;

    // Variables for the look-around camera with the mouse.
    vec3 yaw_axis, pitch_axis;
    glm_vec3_zero(yaw_axis);
    glm_vec3_zero(pitch_axis);
    yaw_axis[1] = 1;
    mat4 rot;
    float ymin = VKL_CAMERA_YMIN;

    switch (controller_type)
    {

    case VKL_CONTROLLER_FPS:
    case VKL_CONTROLLER_FLY:
        if (mouse->cur_state == VKL_MOUSE_STATE_DRAG)
        {
            // Change the camera orientation with the mouse.
            vec2 mouse_delta;
            _mouse_move_delta(mouse, panel->viewport, mouse_delta);
            camera->speed = VKL_CAMERA_SENSITIVITY;

            float incrx = mouse_delta[0] * camera->speed;
            float incry = mouse_delta[1] * camera->speed;

            glm_rotate_make(rot, incrx, yaw_axis);
            glm_mat4_mulv3(rot, camera->forward, 1, camera->forward);

            glm_vec3_crossn(camera->up, camera->forward, pitch_axis);
            if ((camera->forward[1] > max_pitch && incry > 0) ||
                (camera->forward[1] < -max_pitch && incry < 0))
                incry = 0;
            glm_rotate_make(rot, incry, pitch_axis);
            glm_mat4_mulv3(rot, camera->forward, 1, camera->forward);
        }

        // Change the camera elevation with the mouse wheel.
        if (mouse->cur_state == VKL_MOUSE_STATE_WHEEL)
        {
            camera->target[1] -= mouse->wheel_delta[1];
        }

        // Arrow keys navigation.
        vec2 forward_plane;
        forward_plane[0] = camera->forward[0];
        forward_plane[1] = camera->forward[2];
        glm_vec2_normalize(forward_plane);

        if (keyboard->key == VKL_KEY_UP)
        {
            if (controller_type == VKL_CONTROLLER_FPS)
            {
                camera->target[0] += increment * forward_plane[0];
                camera->target[2] += increment * forward_plane[1];
            }
            else if (controller_type == VKL_CONTROLLER_FLY)
            {
                camera->target[0] += increment * camera->forward[0];
                camera->target[1] += increment * camera->forward[1];
                camera->target[2] += increment * camera->forward[2];
            }
        }
        else if (keyboard->key == VKL_KEY_DOWN)
        {
            if (controller_type == VKL_CONTROLLER_FPS)
            {
                camera->target[0] -= increment * forward_plane[0];
                camera->target[2] -= increment * forward_plane[1];
            }
            else if (controller_type == VKL_CONTROLLER_FLY)
            {
                camera->target[0] -= increment * camera->forward[0];
                camera->target[1] -= increment * camera->forward[1];
                camera->target[2] -= increment * camera->forward[2];
            }
        }
        else if (keyboard->key == VKL_KEY_RIGHT)
        {
            camera->target[0] -= -increment * camera->forward[2];
            camera->target[2] -= increment * camera->forward[0];
        }
        else if (keyboard->key == VKL_KEY_LEFT)
        {
            camera->target[0] += -increment * camera->forward[2];
            camera->target[2] += increment * camera->forward[0];
        }

        // Smooth move.
        const double alpha = 10 * scene->canvas->dt;
        camera->eye[0] += alpha * (camera->target[0] - camera->eye[0]);
        camera->eye[1] += alpha * (camera->target[1] - camera->eye[1]);
        camera->eye[2] += alpha * (camera->target[2] - camera->eye[2]);
        break;

    case VKL_CONTROLLER_AUTOROTATE:
        camera->speed = VKL_CAMERA_SPEED * scene->canvas->dt;
        glm_rotate_make(rot, camera->speed, yaw_axis);
        glm_mat4_mulv3(rot, camera->eye, 1, camera->eye);
        glm_mat4_mulv3(rot, camera->forward, 1, camera->forward);
        break;

    default:
        break;
    }

    // Prevent going below y=0 plane.
    if (controller_type == VKL_CONTROLLER_FPS)
        ymin = 0;

    camera->eye[1] = CLIP(camera->eye[1], ymin, VKL_CAMERA_YMAX);
    camera->target[1] = CLIP(camera->target[1], ymin, VKL_CAMERA_YMAX);

    // Reset the camera on double click.
    if (mouse->cur_state == VKL_MOUSE_STATE_DOUBLE_CLICK)
    {
        _reset_camera(camera);
        panel->status = VKL_PANEL_STATUS_RESET;
    }

    glm_vec3_normalize(camera->forward);
    glm_look(camera->eye, camera->forward, camera->up, camera->mvp.view);
    vkl_mvp_set_proj_3D(panel, viewport_type, &camera->mvp);
    vkl_mvp_upload(panel, viewport_type, &camera->mvp);
    vkl_mvp_finalize(scene);
}
*/

static VklCamera _camera(VklInteractType type)
{
    VklCamera c = {0};
    // TODO
    return c;
}

static void _camera_callback(
    VklInteract* interact, VklViewport viewport, VklMouseState* mouse, VklKeyboardState* keyboard)
{
    ASSERT(interact != NULL);
    switch (interact->type)
    {

    case VKL_INTERACT_PANZOOM:
        interact->u.p = _panzoom();
        break;

    case VKL_INTERACT_ARCBALL:
        interact->u.a = _arcball();
        break;

    case VKL_INTERACT_FLY:
    case VKL_INTERACT_FPS:
    case VKL_INTERACT_TURNTABLE:
        interact->u.c = _camera(interact->type);
        break;
    default:
        break;
    }
}



/*************************************************************************************************/
/*  Interact                                                                                     */
/*************************************************************************************************/

VklInteract vkl_interact(VklCanvas* canvas, void* user_data)
{
    ASSERT(canvas != NULL);
    VklInteract interact = {0};
    interact.canvas = canvas;
    interact.user_data = user_data;
    return interact;
}



void vkl_interact_callback(VklInteract* interact, VklInteractCallback callback)
{
    ASSERT(interact != NULL);
    interact->callback = callback;
}



VklInteract vkl_interact_builtin(VklCanvas* canvas, VklInteractType type)
{
    VklInteract interact = vkl_interact(canvas, NULL);
    interact.type = type;
    switch (type)
    {
    case VKL_INTERACT_PANZOOM:
        interact.callback = _panzoom_callback;
        interact.u.p = _panzoom();
        break;

    case VKL_INTERACT_ARCBALL:
        interact.callback = _arcball_callback;
        interact.u.a = _arcball();
        break;

    case VKL_INTERACT_FLY:
    case VKL_INTERACT_FPS:
    case VKL_INTERACT_TURNTABLE:
        interact.callback = _camera_callback;
        interact.u.c = _camera(type);
        break;

    default:
        break;
    }
    return interact;
}



void vkl_interact_update(
    VklInteract* interact, VklViewport viewport, VklMouseState* mouse, VklKeyboardState* keyboard)
{
    ASSERT(interact != NULL);
    if (interact->callback != NULL)
        interact->callback(interact, viewport, mouse, keyboard);
}
