#ifndef WORLD_OCEAN_H
#define WORLD_OCEAN_H

#include "raylib.h"

/* The water under the flooded streets is a real ocean, modelled on how
   they actually work.

   Light falls off exponentially with depth (Beer-Lambert), which is why
   the sea has zones rather than a gradient you can see through. Below the
   sunlit layer nothing photosynthesises, so plants stop and the only
   light is what things make themselves. Temperature drops sharply through
   a thermocline and then stays near freezing. Pressure climbs steadily
   the whole way down.

   Those three curves are the whole design: light decides what you can
   see, cold decides how long you last, pressure decides how deep you can
   go at all. */

typedef enum OceanZone {
    OCEAN_SUNLIT = 0,   /* the flooded sprawl and the shelf   */
    OCEAN_TWILIGHT,     /* dim; bioluminescence starts here   */
    OCEAN_MIDNIGHT,     /* no daylight reaches this at all    */
    OCEAN_ABYSS,        /* vents, and things that live on them */
    OCEAN_ZONE_COUNT
} OceanZone;

/* Depth is measured downward from the current surface. */
float      OceanDepthAt(float worldY);
OceanZone  OceanZoneAtDepth(float depth);
const char *OceanZoneName(OceanZone zone);

/* 1 at the surface, falling exponentially. */
float OceanLight(float depth);

/* 0 in the warm surface layer, 1 once past the thermocline. */
float OceanChill(float depth);

/* Rises with depth; past OCEAN_CRUSH_DEPTH it starts doing damage. */
float OceanPressure(float depth);

#define OCEAN_CRUSH_DEPTH 1500.0f

/* Seafloor height at a chunk boundary. Both neighbours derive it from the
   shared index, so the bathymetry is continuous - the same trick the
   ground surface uses. */
float OceanFloorHeight(int boundaryIndex);

/* The floor under an arbitrary world x, interpolated between the two
   boundaries either side of it: the same line BuildOcean steps its slabs
   along, without the per-slab jitter. Accurate enough to steer by, which
   is what it is for - anything that swims aims to clear this, and the
   collision boxes are what it actually bumps into. */
float OceanFloorAt(float worldX);

#endif /* WORLD_OCEAN_H */
