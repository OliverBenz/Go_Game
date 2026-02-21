#pragma once

#include <vector>

namespace go::vision::core {

/*! Reconstruct the NxN Go board grid from candidate line-center detections.
 *  The rectified board image provides sorted candidate line centers per axis.
 *  Detections may be missing grid lines or contains extra lines (most commonly the physical board border).
 *  We fit an equally-spaced 1D lattice for each N in {9, 13, 19} and select the best joint fit across both axes.
 *
 * \param [in]  vCenters Sorted x-coordinates of candidate vertical line centers (pixels, rectified image space).
 * \param [in]  hCenters Sorted y-coordinates of candidate horizontal line centers (pixels, rectified image space).
 * \param [out] vGrid    Output x-coordinates of the selected vertical grid lines (size = N).
 * \param [out] hGrid    Output y-coordinates of the selected horizontal grid lines (size = N).
 * \return      True if a consistent NxN grid (N in {9,13,19}) was found; false otherwise.
 */
bool findGrid(const std::vector<double>& vCenters, const std::vector<double>& hCenters, std::vector<double>& vGrid, std::vector<double>& hGrid);

/*! Reconstruct the NxN Go board grid from candidate line-center detections using explicit candidate board sizes.
 *  This is primarily used for debug validation paths where N is already known and should be constrained.
 *
 * \param [in]  vCenters     Sorted x-coordinates of candidate vertical line centers (pixels, rectified image space).
 * \param [in]  hCenters     Sorted y-coordinates of candidate horizontal line centers (pixels, rectified image space).
 * \param [out] vGrid        Output x-coordinates of the selected vertical grid lines (size = N).
 * \param [out] hGrid        Output y-coordinates of the selected horizontal grid lines (size = N).
 * \param [in]  candidateNs  Allowed board sizes to test (subset of {9,13,19}).
 * \return      True if a consistent NxN grid was found; false otherwise.
 */
bool findGrid(const std::vector<double>& vCenters, const std::vector<double>& hCenters, std::vector<double>& vGrid, std::vector<double>& hGrid,
              const std::vector<std::size_t>& candidateNs);

} // namespace go::camera
