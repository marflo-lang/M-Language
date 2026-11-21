
var a = 5
var b = 5 * 6
var c = true or 5
var d = not a and b or c
var e = 0

if d {
    for i = 0; i < 10 {
        e++
    }
} else {
    var t = 0
    while t < 10 {
        e *= t
    }
}

function foo() {

}

var li = {bar3 = {}}
var li2 = []

function li.bar() {

}

function li["bar2"]() {

}

function li.bar3.bar() {

}

function li.bar3["bar2"]() {

}

function li2[1]() {

}

function li[foo()]() {

}
