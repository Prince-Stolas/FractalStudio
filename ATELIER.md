Use a cross-platform Qt 6 UI and portable OpenGL rendering; do not introduce Windows-specific UI code. Mandelbrot and Julia are editable equation presets in a general fractal playground, not fixed rendering modes.
Build static binaries by default, including Qt and compiler runtimes; system graphics/OS libraries may remain dynamic.
Treat a, b, and c uniformly with Constant, Mouse picked, and Pixel coordinate sources. Initial z uses a source dropdown (also Equation), not a user-facing z0 variable; show only source-relevant inputs and allow customizable picking modifiers.
Use numbered views, never lettered names. Julia conversion must preserve the target view's colour settings.
Maintain a local Git repository and commit completed changes with detailed, explanatory commit bodies from now on. Do not push unless asked. Use Stolas <stolas@localhost> as the commit identity via per-command environment variables when no identity is configured; do not change Git configuration.
