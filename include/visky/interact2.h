#ifndef VKL_INTERACT_HEADER
#define VKL_INTERACT_HEADER

#include "canvas.h"
#include "graphics.h"



/*************************************************************************************************/
/*  Typedefs                                                                                     */
/*************************************************************************************************/

typedef struct VklMouse VklMouse;
typedef struct VklKeyboard VklKeyboard;
typedef struct VklPanzoom VklPanzoom;
typedef struct VklArcball VklArcball;
typedef struct VklCamera VklCamera;
typedef struct VklInteract VklInteract;
typedef union VklInteractUnion VklInteractUnion;
typedef struct VklMouseLocal VklMouseLocal;



/*************************************************************************************************/
/*  Enums                                                                                        */
/*************************************************************************************************/

// Mouse state type.
typedef enum
{
    VKL_MOUSE_STATE_INACTIVE,
    VKL_MOUSE_STATE_DRAG,
    VKL_MOUSE_STATE_WHEEL,
    VKL_MOUSE_STATE_CLICK,
    VKL_MOUSE_STATE_DOUBLE_CLICK,
    VKL_MOUSE_STATE_CAPTURE,
} VklMouseStateType;



// Key state type.
typedef enum
{
    VKL_KEYBOARD_STATE_INACTIVE,
    VKL_KEYBOARD_STATE_ACTIVE,
    VKL_KEYBOARD_STATE_CAPTURE,
} VklKeyboardStateType;



// Interact type.
typedef enum
{
    VKL_INTERACT_NONE,
    VKL_INTERACT_PANZOOM,
    VKL_INTERACT_ARCBALL,
    VKL_INTERACT_TURNTABLE,
    VKL_INTERACT_FLY,
    VKL_INTERACT_FPS,
} VklInteractType;



// Interact callback.
typedef void (*VklInteractCallback)(
    VklInteract* interact, VklViewport viewport, //
    VklMouse* mouse, VklKeyboard* keyboard);



/*************************************************************************************************/
/*  Mouse and keyboard states                                                                    */
/*************************************************************************************************/

struct VklMouse
{
    VklMouseButton button;
    vec2 press_pos;
    vec2 last_pos;
    vec2 cur_pos;
    vec2 wheel_delta;
    float shift_length;

    VklMouseStateType prev_state;
    VklMouseStateType cur_state;

    double press_time;
    double click_time;
};



// In normalize coordinates [-1, +1]
struct VklMouseLocal
{
    vec2 press_pos;
    vec2 last_pos;
    vec2 cur_pos;
    vec2 delta;
};



struct VklKeyboard
{
    VklKeyCode key_code;
    int modifiers;

    VklKeyboardStateType prev_state;
    VklKeyboardStateType cur_state;

    double press_time;
};



/*************************************************************************************************/
/*  Panzoom                                                                                      */
/*************************************************************************************************/

struct VklPanzoom
{
    vec3 camera_pos;
    vec2 zoom;
    bool lim_reached[2];
};



/*************************************************************************************************/
/*  Arcball                                                                                      */
/*************************************************************************************************/

struct VklArcball
{
    mat4 center_translation, translation;
    versor rotation;
    mat4 mat_arcball;
    mat4 mat_user;
    vec3 eye_init;
};



/*************************************************************************************************/
/*  Camera                                                                                       */
/*************************************************************************************************/

struct VklCamera
{
    vec3 eye; // smoothly follows target
    vec3 forward;
    vec3 up;
    vec3 target; // requested eye position modified by mouse and keyboard, used for smooth move
    float speed;
};



/*************************************************************************************************/
/*  Interact                                                                                     */
/*************************************************************************************************/

union VklInteractUnion
{
    VklPanzoom p;
    VklArcball a;
    VklCamera c;
};



struct VklInteract
{
    VklInteractType type;
    VklCanvas* canvas;
    VklMouseLocal mouse_local;
    VklInteractCallback callback;
    VklMVP mvp;
    VklInteractUnion u;
    void* user_data;
};



/*************************************************************************************************/
/*  Functions                                                                                    */
/*************************************************************************************************/

VKY_EXPORT VklMouse vkl_mouse(void);

VKY_EXPORT void vkl_mouse_reset(VklMouse* mouse);

VKY_EXPORT void vkl_mouse_event(VklMouse* mouse, VklCanvas* canvas, VklEvent ev);

VKY_EXPORT void vkl_mouse_local(
    VklMouse* mouse, VklMouseLocal* mouse_local, VklCanvas* canvas, VklViewport viewport);



VKY_EXPORT VklKeyboard vkl_keyboard(void);

VKY_EXPORT void vkl_keyboard_reset(VklKeyboard* keyboard);

VKY_EXPORT void vkl_keyboard_event(VklKeyboard* keyboard, VklCanvas* canvas, VklEvent ev);



VKY_EXPORT VklInteract vkl_interact(VklCanvas* canvas, void* user_data);

VKY_EXPORT void vkl_interact_callback(VklInteract* interact, VklInteractCallback callback);

VKY_EXPORT VklInteract vkl_interact_builtin(VklCanvas* canvas, VklInteractType type);

VKY_EXPORT void vkl_interact_update(
    VklInteract* interact, VklViewport viewport, VklMouse* mouse, VklKeyboard* keyboard);


#endif
