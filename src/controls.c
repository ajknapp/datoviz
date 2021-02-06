#include <inttypes.h>

#include "../include/datoviz/canvas.h"
#include "../include/datoviz/controls.h"

// NOTE: This file should NOT import gui.h (C++)



/*************************************************************************************************/
/*  Gui controls API                                                                             */
/*************************************************************************************************/

DvzGui* dvz_gui(DvzCanvas* canvas, const char* title, int flags)
{
    ASSERT(canvas != NULL);
    DvzGui* gui = (DvzGui*)dvz_container_alloc(&canvas->guis);
    gui->canvas = canvas;
    gui->title = title;
    gui->flags = flags;
    dvz_obj_init(&gui->obj);
    return gui;
}



static DvzGuiControl* _add_control(
    DvzGui* gui, DvzGuiControlType type, const char* name, VkDeviceSize item_size, void* value)
{
    ASSERT(gui != NULL);

    DvzGuiControl* control = &gui->controls[gui->control_count++];
    control->gui = gui;
    control->name = name;
    control->type = type;
    control->value = calloc(1, item_size);
    memcpy(control->value, value, item_size);
    return control;
}



void dvz_gui_slider_float(DvzGui* gui, const char* name, float vmin, float vmax, float value)
{
    ASSERT(gui != NULL);
    ASSERT(vmin < vmax);
    value = CLIP(value, vmin, vmax);

    DvzGuiControl* control =
        _add_control(gui, DVZ_GUI_CONTROL_SLIDER_FLOAT, name, sizeof(float), &value);
    control->u.sf.vmin = vmin;
    control->u.sf.vmax = vmax;
}



void dvz_gui_slider_int(DvzGui* gui, const char* name, int vmin, int vmax, int value)
{
    ASSERT(gui != NULL);
    ASSERT(vmin < vmax);
    value = CLIP(value, vmin, vmax);

    DvzGuiControl* control =
        _add_control(gui, DVZ_GUI_CONTROL_SLIDER_INT, name, sizeof(int), &value);
    control->u.si.vmin = vmin;
    control->u.si.vmax = vmax;
}


void dvz_gui_label(DvzGui* gui, const char* name, char* text)
{
    ASSERT(gui != NULL);
    ASSERT(name != NULL);
    ASSERT(text != NULL);
    DvzGuiControl* control =
        _add_control(gui, DVZ_GUI_CONTROL_LABEL, name, strlen(text) * sizeof(char), text);
    ASSERT(control != NULL);
}


void dvz_gui_destroy(DvzGui* gui)
{
    ASSERT(gui != NULL);
    for (uint32_t i = 0; i < gui->control_count; i++)
    {
        FREE(gui->controls[i].value);
    }
}
