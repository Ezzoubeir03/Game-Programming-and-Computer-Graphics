Curve Lab - Bezier curve + de Casteljau animation (Win32 + OpenGL, no GLUT)
===========================================================================

HOW TO RUN (Visual Studio 2010)
  1. Extract the zip.
  2. Double-click CurveLab.sln
  3. Select Release (or Debug) + Win32, press Ctrl+F5.

CURVE TYPES (keys 1 / 2 / 3)
  1  Bezier curve - degree = (points - 1), drawn with de Casteljau's algorithm
  2  Catmull-Rom spline - passes through every point
  3  Uniform cubic B-spline - smooth, pulled toward the points

MOUSE
  Click empty space  add a control point (max 24)
  Drag a point       move it
  Right-click point  delete it (min 2)

KEYS
  Space play/pause t    Left/Right set t by hand (hold to reach 0 or 1)
  0 = jump to t=0 (paused)   5 = jump to t=0.5 (paused)
  T show end tangents (B'(0), B'(1))    D construction on/off
  L control polygon     G grid     C clear     R reset example     Esc exit

DE CASTELJAU IDEA
  Repeatedly interpolate between neighbouring points at the same t:
  P(k,i) = (1-t)*P(k-1,i) + t*P(k-1,i+1). The last single point is C(t).
