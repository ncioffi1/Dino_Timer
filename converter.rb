#

arr = [0, 0, 0, 0, 0, 0]

hrs = arr[0] * 10 + arr[1]
mins = arr[2] * 10 + arr[3]
secs = arr[4] * 10 + arr[5]

hrs = 00
mins = 70
secs = 70

if secs > 60
    secs -= 60
    mins += 1
end

if mins > 60
    mins -= 60
    hrs += 1
end

if hrs > 24
    hrs = 24
end

