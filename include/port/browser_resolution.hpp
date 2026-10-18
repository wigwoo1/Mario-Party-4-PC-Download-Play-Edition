#pragma once

#include <cstdint>

namespace partyboard {
constexpr bool browserResolutionMatchesAspect(int width,int height,int numerator,int denominator) {
    const std::int64_t difference=static_cast<std::int64_t>(width)*denominator-
        static_cast<std::int64_t>(height)*numerator;
    const std::int64_t absoluteDifference=difference<0?-difference:difference;
    // Allow less than 0.003 ratio error to account for integer-rounded 21:9.
    return absoluteDifference*1000<static_cast<std::int64_t>(3)*height*denominator;
}

constexpr bool browserResolutionSupported(int width,int height) {
    const bool supportedWidth=width==640||width==960||width==1280||width==1920||width==2560;
    const bool supportedHeight=height>=240&&height<=1920;
    return supportedWidth&&supportedHeight&&
        (browserResolutionMatchesAspect(width,height,4,3)||
         browserResolutionMatchesAspect(width,height,16,9)||
         browserResolutionMatchesAspect(width,height,16,10)||
         browserResolutionMatchesAspect(width,height,21,9));
}
} // namespace partyboard
