typedef struct Range2f
{
    Vector2 min;
    Vector2 max;
} Range2f;

inline Range2f range2f_make(Vector2 min, Vector2 max) { return (Range2f){min, max}; }

Range2f range2f_shift(Range2f r, Vector2 shift)
{
    r.min = v2_add(r.min, shift);
    r.max = v2_add(r.max, shift);
    return r;
}

Vector2 range2f_size(Range2f range)
{
    Vector2 size = {0};
    size = v2_sub(range.min, range.max);
    size.x = fabsf(size.x);
    size.y = fabsf(size.y);
    return size;
}

/// @brief make range that assumes bottom center pivot
Range2f range2f_make_bottom_center(Vector2 size)
{
    Range2f range = {0};
    range.max = size;
    range = range2f_shift(range, v2(size.x * -0.5, 0.0));
    return range;
}

Vector2 range2f_get_center(Range2f r)
{
    return (Vector2){(r.max.x - r.min.x) * 0.5 + r.min.x, (r.max.y - r.min.y) * 0.5 + r.min.y};
}

Range2f range2f_make_bottom_left(Vector2 pos, Vector2 size)
{
    return (Range2f){pos, v2_add(pos, size)};
}

Range2f range2f_make_top_right(Vector2 pos, Vector2 size)
{
    return (Range2f){v2_sub(pos, size), pos};
}

Range2f range2f_make_bottom_right(Vector2 pos, Vector2 size)
{
    return (Range2f){v2(pos.x - size.x, pos.y), v2(pos.x, pos.y + size.y)};
}

Range2f range2f_make_center_right(Vector2 pos, Vector2 size)
{
    return (Range2f){v2(pos.x - size.x, pos.y - size.y * 0.5), v2(pos.x, pos.y + size.y * 0.5)};
}

bool range2f_contains_v2(Range2f range, Vector2 v)
{
    return v.x >= range.min.x && v.x <= range.max.x && v.y >= range.min.y && v.y <= range.max.y;
}

/// @brief check that the 2 rectangles have sides that overlap in any way
bool range2f_AABB(Range2f range1, Range2f range2)
{
    Vector2 range1_botLeft = range1.min;
    Vector2 range1_topLeft = v2(range1.min.x, range1.max.y);
    Vector2 range1_botright = v2(range1.max.x, range1.min.y);
    Vector2 range1_topRight = range1.max;

    Vector2 range2_botLeft = range2.min;
    Vector2 range2_topLeft = v2(range2.min.x, range2.max.y);
    Vector2 range2_botright = v2(range2.max.x, range2.min.y);
    Vector2 range2_topRight = range2.max;

    bool result = range1.min.x < range2.max.x &&
                range1.max.x > range2.min.x &&
                range1.min.y < range2.max.y &&
                range1.max.y > range2.min.y;
#ifdef debug
    if (result)
    {
        Vector4 collidedColor = COLOR_RED;
        collidedColor.a = 0.4;
        draw_rect(range2.min, range2f_size(range2), collidedColor);
        draw_rect(range1.min, range2f_size(range1), collidedColor);
    }
    else
    {
        Vector4 collidedColor = COLOR_RED;
        collidedColor.a = 0.2;
        draw_rect(range2.min, range2f_size(range2), collidedColor);
        draw_rect(range1.min, range2f_size(range1), collidedColor);
    }
#endif
    return result;
}