# Triangulation of a given polygon
This file requires raylib to compile. Given a text file describing a list of
walls/lines as start/end coordinates, it will translate the shape into
triangles.
Shapes should be given in clockwise order, and holes must be given
anti-clockwise after the shape is closed then given in counter-clockwise order.
For an example see simplehole2.txt

for now, only a single hole is supported, and may crash if given in a certain rotation

Please excuse that this displays the shape upside-down or out of bounds, I cba
to center the display. If someone wants to submit a pr that fixes that, go for
it.

This will be incorporated into the renderer for a certain game, stay tuned.
