#include "ui.h"

internal void set_hot(UIState *ui, UIID uiID)
{
    if (!(ui->active.ID > 0) && uiID.layer >= ui->hotThisFrame.layer)
    {
        ui->hotThisFrame = uiID;
    }
}

internal void set_active(UIState *ui, UIID uiID)
{
    ui->active = uiID;
}

internal void set_inactive(UIState *ui)
{
    ui->active = {};
}

internal float get_text_width(char *text)
{
    float result = 0.0f;
    float highestTextWidth = 0.0f;

    if (text)
    {
        while (char c = *(text++))
        {
            if (c == '\n')
            {
                if (highestTextWidth < result)
                {
                    highestTextWidth = result;
                }
                continue;
                ;
            }

            result += 15.0f;
        }

        if (result < highestTextWidth)
        {
            result = highestTextWidth;
        }
    }

    return result;
}

internal UIElement *create_ui_element(UIState *ui, AssetTypeID assetTypeID,
                                      uint32_t ID, Rect rect, char *text)
{
    UIElement *e = 0;

    if (ui->uiElementCount < MAX_UI_ELEMENTS)
    {
        e = &ui->uiElements[ui->uiElementCount++];
        *e = {}; // Initialize to zero

        e->assetTypeID = assetTypeID;
        e->rect = rect;
        e->ID = {ID, ui->globalLayer};
        e->text = text;
    }
    else
    {
        CAKEZ_ASSERT(0, "Reached Maximum amount of UI Elements");
    }

    return e;
}

bool is_hot(UIState *ui, uint32_t ID)
{
    return ui->hotLastFrame.ID > 0 && ui->hotLastFrame.ID == ID;
}

bool is_active(UIState *ui, uint32_t ID)
{
    return ui->active.ID > 0 && ui->active.ID == ID;
}

void update_ui(UIState *ui)
{
    ui->labelCount = 0;
    ui->uiElementCount = 0;
    ui->hotLastFrame = ui->hotThisFrame;
}

void do_text(UIState *ui, Vec2 pos, char *text)
{
    if (ui->labelCount < MAX_LABELS)
    {
        Label *l = &ui->labels[ui->labelCount++];
        *l = {};

        l->text = text;
        l->pos = pos;
    }
    else
    {
        CAKEZ_ASSERT(0, "Reached maximum amount of Labels!");
    }
}

void do_number(UIState* ui, Vec2 pos, int number)
{
    if (ui->labelCount < MAX_LABELS)
    {
        Label *l = &ui->labels[ui->labelCount++];
        *l = {};

        l->number = number;
        l->pos = pos;
    }
    else
    {
        CAKEZ_ASSERT(0, "Reached maximum amount of Labels!");
    }
}

bool do_button(UIState *ui, InputState *input, AssetTypeID assetTypeID, uint32_t ID, Rect rect, char *text)
{
    bool result = false;

    UIElement *e = create_ui_element(ui, assetTypeID, ID, rect, text);

    if (e)
    {
        Vec2 textPos = rect.pos + Vec2{0.0f, 9.0f};

        if (is_active(ui, ID))
        {
            if (key_released_this_frame(input, KEY_LEFT_MOUSE))
            {
                if (point_in_rect(input->mousePos, rect))
                {
                    result = true;
                }

                set_inactive(ui);
            }

            e->animationIdx = 2;
            textPos.y += 3.0f;
        }
        else if (is_hot(ui, ID))
        {
            if (key_pressed_this_frame(input, KEY_LEFT_MOUSE))
            {
                set_active(ui, e->ID);
            }

            e->animationIdx = 1;
        }

        if (point_in_rect(input->mousePos, rect))
        {
            set_hot(ui, e->ID);
        }

        if (text)
        {
            float textWidth = get_text_width(text);
            // Center
            textPos.x += (rect.size.x - textWidth) / 2.0f;

            do_text(ui, textPos, text);
        }
    }

    return result;
}
