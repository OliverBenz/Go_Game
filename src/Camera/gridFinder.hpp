#pragma once

#include <vector>

namespace go::camera {

//! Determine the true Go board grid lines from clustered vertical and horizontal line candidates
//! by fitting arithmetic progressions (9x9, 13x13, 19x19) and selecting the best structural match.
//! \param vCenters [in]  Sorted x-coordinates of candidate vertical line centers (warped image space).
//! \param hCenters [in]  Sorted y-coordinates of candidate horizontal line centers (warped image space).
//! \param vGrid    [out] Output x-coordinates of the selected vertical grid lines (size = board dimension).
//! \param hGrid    [out] Output y-coordinates of the selected horizontal grid lines (size = board dimension).
//! \return         True if a consistent NxN grid (N ∈ {9,13,19}) was found; false otherwise.
bool findGrid(
    const std::vector<double>& vCenters, 
    const std::vector<double>& hCenters,
    std::vector<double>& vGrid, 
    std::vector<double>& hGrid
);

}