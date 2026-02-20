X = 11
Y = 11
Z = 11
Name = "Giant enemy ball"
Desc = "Quick, you gotta defeat the giant enemy ball! (pssst... use this for the color: 'color.fromRGB(0.5, y / Y, 0.5))'"
local center = {
    x = math.floor(X / 2),
    y = math.floor(Y / 2),
    z = math.floor(Z / 2),
}
local function dist(p1, p2)
    local tmp = {}
    tmp[0] = (p1.x - p2.x) ^ 2
    tmp[1] = (p1.y - p2.y) ^ 2
    tmp[2] = (p1.z - p2.z) ^ 2
    local sum = tmp[0] + tmp[1] + tmp[2]
    return math.sqrt(sum)
end
function Generate()
    local p = {
        x = x,
        y = y,
        z = z,
    }
    if (dist(p, center) <= 4) then
        Color = color.fromRGB(0.5, y / Y, 0.5)
    end
end
