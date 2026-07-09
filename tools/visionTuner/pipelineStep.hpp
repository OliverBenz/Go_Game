#pragma once

namespace tengen {

enum class PipelineStep { FindBoard = 0, ConstructGeometry, FindStones, All };

enum class ImageSource { Photo, Video };
enum class VideoMode { Live, Manual };

} // namespace tengen
