#version 120

#include "terrain_common.h"

#include "common/los_vertex.h"
#include "common/shadows_vertex.h"
#include "common/vertex.h"

VERTEX_INPUT_ATTRIBUTE(0, vec3, a_vertex);
VERTEX_INPUT_ATTRIBUTE(1, vec3, a_normal);
#if DECAL
VERTEX_INPUT_ATTRIBUTE(2, vec2, a_uv0);
#endif
#if BLEND
VERTEX_INPUT_ATTRIBUTE(3, vec2, a_uv1);
#endif

void main()
{
  vec4 position = vec4(a_vertex, 1.0);

  OUTPUT_VERTEX_POSITION(transform * position);

  v_lighting = clamp(-dot(a_normal, sunDir), 0.0, 1.0) * sunColor;

  #if DECAL
    v_tex.xy = a_uv0;
  #else

    #if USE_TRIPLANAR
      v_tex = a_vertex;
    #else
      // Compute texcoords from position and terrain-texture-dependent transform
      float c = textureTransform.x;
      float s = -textureTransform.y;
      v_tex = vec2(a_vertex.x * c + a_vertex.z * -s, a_vertex.x * -s + a_vertex.z * -c);
    #endif

    #if GL_ES
      // XXX: Ugly hack to hide some precision issues in GLES
      #if USE_TRIPLANAR
        v_tex = mod(v_tex, vec3(9.0, 9.0, 9.0));
      #else
        v_tex = mod(v_tex, vec2(9.0, 9.0));
      #endif
    #endif
  #endif

  #if BLEND
    v_blend = a_uv1;
  #endif

  calculatePositionInShadowSpace(vec4(a_vertex, 1.0));
#if USE_SPECULAR_MAP || USE_NORMAL_MAP || USE_TRIPLANAR
  v_normal = a_normal;
#endif

  #if USE_NORMAL_MAP || USE_SPECULAR_MAP || USE_TRIPLANAR
    #if USE_NORMAL_MAP
      #if DECAL || USE_TRIPLANAR
        // TODO: this should be fixed for decals/triplanar mapping.
        v_tangent = vec3(1.0, 0.0, 0.0);
      #else
        v_tangent = normalize(vec3(c, 0.0, -s));
      #endif
      v_bitangent = cross(v_normal, v_tangent);
    #endif

    #if USE_SPECULAR_MAP
      vec3 eyeVec = cameraPos.xyz - position.xyz;
      vec3 sunVec = -sunDir;
      v_half = normalize(sunVec + normalize(eyeVec));
    #endif
  #endif

#if !IGNORE_LOS
  v_los = calculateLOSCoordinates(a_vertex.xz, losTransform);
#endif
}
