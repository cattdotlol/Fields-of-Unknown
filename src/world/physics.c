#include "world/physics.h"
#include "world/terrain.h"

void BodyInit(Body *b, Vector2 pos, float width, float height)
{
    b->pos = pos;
    b->prevPos = pos;
    b->vel = (Vector2){ 0.0f, 0.0f };
    b->width = width;
    b->height = height;
    b->grounded = false;
}

Rectangle BodyRectAt(const Body *b, Vector2 pos, float height)
{
    return (Rectangle){ pos.x - b->width * 0.5f, pos.y - height, b->width, height };
}

Rectangle BodyRect(const Body *b)
{
    return BodyRectAt(b, b->pos, b->height);
}

void BodyBeginTick(Body *b)
{
    b->prevPos = b->pos;
}

void BodyApplyGravity(Body *b, float gravity, float maxFall, float dt)
{
    b->vel.y += gravity * dt;
    if (b->vel.y > maxFall) b->vel.y = maxFall;
}

static void MoveX(Body *b, float dt)
{
    b->pos.x += b->vel.x * dt;

    Rectangle box = BodyRect(b);

    for (int i = 0; i < TerrainCount(); i++)
    {
        Rectangle s = TerrainSolid(i);
        if (!CheckCollisionRecs(box, s)) continue;

        if (b->vel.x > 0.0f)      b->pos.x = s.x - b->width * 0.5f;
        else if (b->vel.x < 0.0f) b->pos.x = s.x + s.width + b->width * 0.5f;

        b->vel.x = 0.0f;
        box = BodyRect(b);
    }

    /* No clamp: the world has no edges to run into. */
}

static void MoveY(Body *b, float dt)
{
    b->pos.y += b->vel.y * dt;
    b->grounded = false;

    Rectangle box = BodyRect(b);

    for (int i = 0; i < TerrainCount(); i++)
    {
        Rectangle s = TerrainSolid(i);
        if (!CheckCollisionRecs(box, s)) continue;

        if (b->vel.y > 0.0f)
        {
            b->pos.y = s.y;
            b->grounded = true;
        }
        else if (b->vel.y < 0.0f)
        {
            b->pos.y = s.y + s.height + b->height;
        }

        b->vel.y = 0.0f;
        box = BodyRect(b);
    }
}

void BodyMove(Body *b, float dt)
{
    MoveX(b, dt);
    MoveY(b, dt);
}

Vector2 BodyRenderPos(const Body *b, float alpha)
{
    return (Vector2){
        b->prevPos.x + (b->pos.x - b->prevPos.x) * alpha,
        b->prevPos.y + (b->pos.y - b->prevPos.y) * alpha,
    };
}
