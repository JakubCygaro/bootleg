# TODO
- dedect cube under cursor and give its coordinates and maybe color information as a tooltip
- ADD LEVELS
- do some code cleanup and optimizing with the buffer
- special sequences for syntax highlighting, BLANK would cause the buffer to skip rendering
 glyphs and so something like this:
```
^^[RED] THIS TEXT IS RED ^^[END]
```
would make ^^[RED] and ^^[END] invisible and the text in-between would be coloured red
