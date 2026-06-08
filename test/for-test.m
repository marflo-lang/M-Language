var r = 1
for i = 0, i < 10, i++
    r += i

for i = 0; i < 10
    r += i

#for i = "0", i < "10"; i += "1"
#    r += i

for i = "0"; 1_00 > i; ++i
    r += i

for i = 1_00; i > 0, --i
    r += i

for i = "1_00"; 0 < i, i--
    r += i

for i = 1e1, i > 0e0
    r+=1

for i = 1e0; i < 1e2; i = 1e0 + i
    r+=i
    