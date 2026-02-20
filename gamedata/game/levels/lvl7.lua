X = 10
Y = 10
Z = 10
Name = "Me house"
Desc =
"I'm going up on the mountain for the rest o'my life. Before they take my life, before they take my wild life. Build me a cabin!"
function Generate()
    roof = y > 5 and (z ~= 0 and z ~= 9)
    walls = (x == 1) or
        (z == 1 and x ~= 1) or
        (x == 8) or
        (z == 8 and x ~= 8)
    walls = walls and y <= 5

    door = (z == 8) and
        (x == 4 or x == 5) and
        (y < 3)

    windows = (x == 8 or x == 1) and
        (z == 4 or z == 5) and
        (y <= 3 and y >= 1) and
        not door


    if (walls) then
        if (y % 2 == 0) then
            Color = BROWN
        else
            Color = GRAY
        end
    end

    if (windows) then
        Color = BLUE
    end

    if (door) then
        Color = BLANK
    end

    local xForSlope = x
    if (x < 5) then
        xForSlope = x + 1
    end

    roofSlope =
        (math.abs(xForSlope - X / 2) <
            math.floor(Y - y))

    if (roof and roofSlope) then
        Color = RED
    end
end
