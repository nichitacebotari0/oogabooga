typedef float32 easing_function(float32 number);
// always forward define easing functions to conform to signature

easing_function easeOutQuart;
float32 easeOutQuart(float32 x)
{
    return 1 - pow(1 - x, 4);
}

easing_function easeOutQuartReverse;
float32 easeOutQuartReverse(float32 x)
{
    return pow(1 - x, 4);
}

easing_function easeInQuart;
float32 easeInQuart(float32 x)
{
    return x * x * x * x;
}

easing_function easeInQuartReverse;
float32 easeInQuartReverse(float32 x)
{
    return 1 - x * x * x * x;
}
