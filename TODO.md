# TODO

- Virtual file system game assets management with meurlgys
```
    /data
        |-player/
        |       |-levels/
        |-game/
              |-levels/
              |-config

```

- Raw level format
```
header: (key-value pairs defining the attributes of the level)
X = 3
Y = 3
Z = 3
data: (continuos arrays of values separated by two newlines, they define the consecutive cube layers from y = 0 to y = Y - 1)
(format of the data is either hex color values 0xffffffff or predefined color aliases such as 'r[ed]' 'g[reen]' 'b[lue]' 'o')
// X ->
r r r
g g g
r r r

r g r
g g g
r g r

r r r
g g g
r r r
```
