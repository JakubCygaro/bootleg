X = 9
Y = 9
Z = 9
Name = "Non-binary tree"
Desc = "Since a binary tree would have a branch or two"
function Generate()
    if(x == math.floor(X / 2)
            and z == math.floor(Z / 2)
            and y < Y - 1) then
        Color = BROWN
    elseif(y > 5 and
        math.abs(x - math.floor(X / 2)) < 3 and
        math.abs(z - math.floor(Z / 2)) < 3) then
        Color = GREEN
    end
end
