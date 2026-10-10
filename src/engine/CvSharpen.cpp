#include "CvSharpen.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/photo.hpp>
#include <vector>
#include <algorithm>

namespace {

// QImage::Format_ARGB32 stores each pixel as the 32-bit word 0xAARRGGBB;
// on the little-endian byte order used by all realistic target
// platforms that unpacks in memory as B, G, R, A per pixel - exactly
// OpenCV's native BGRA channel order. So the conversion is a reinterpret,
// not a per-pixel channel shuffle.
cv::Mat qimageToBgra(const QImage& img) {
    QImage converted = (img.format() == QImage::Format_ARGB32) ? img : img.convertToFormat(QImage::Format_ARGB32);
    cv::Mat view(converted.height(), converted.width(), CV_8UC4,
                 const_cast<uchar*>(converted.constBits()), converted.bytesPerLine());
    return view.clone(); // own our copy; `converted` may be a temporary
}

QImage bgraToQImage(const cv::Mat& bgra) {
    QImage img(bgra.data, bgra.cols, bgra.rows, int(bgra.step), QImage::Format_ARGB32);
    return img.copy(); // deep copy - `bgra`'s buffer isn't guaranteed to outlive this call
}

} // namespace

namespace CvSharpen {

ImageBuffer unsharpMask(const ImageBuffer& src, double amount, double radiusPx, int threshold) {
    if (src.isNull() || amount <= 0.0) return src;

    cv::Mat bgra = qimageToBgra(src.image());
    std::vector<cv::Mat> channels;
    cv::split(bgra, channels); // B, G, R, A
    cv::Mat alpha = channels[3];
    cv::Mat bgr;
    cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, bgr);

    cv::Mat blurred;
    cv::GaussianBlur(bgr, blurred, cv::Size(0, 0), std::max(0.1, radiusPx));

    cv::Mat diff;
    cv::subtract(bgr, blurred, diff, cv::noArray(), CV_16S);

    if (threshold > 0) {
        // Zero out low-magnitude differences so flat areas/noise aren't
        // amplified - only edges stronger than `threshold` get sharpened.
        cv::Mat absDiff, grayDiff, mask, mask3, gated;
        cv::absdiff(bgr, blurred, absDiff);
        cv::cvtColor(absDiff, grayDiff, cv::COLOR_BGR2GRAY);
        cv::threshold(grayDiff, mask, threshold, 255, cv::THRESH_BINARY);
        cv::cvtColor(mask, mask3, cv::COLOR_GRAY2BGR);
        gated = cv::Mat::zeros(diff.size(), diff.type());
        diff.copyTo(gated, mask3);
        diff = gated;
    }

    cv::Mat bgr16, sharp16, sharpened;
    bgr.convertTo(bgr16, CV_16S);
    cv::addWeighted(bgr16, 1.0, diff, amount, 0.0, sharp16);
    sharp16.convertTo(sharpened, CV_8U); // convertTo saturates to 0..255 automatically

    std::vector<cv::Mat> outChannels;
    cv::split(sharpened, outChannels);
    outChannels.push_back(alpha);
    cv::Mat outBgra;
    cv::merge(outChannels, outBgra);

    return ImageBuffer(bgraToQImage(outBgra));
}

ImageBuffer smartSharpen(const ImageBuffer& src, double amount, double radiusPx, double strength) {
    if (src.isNull()) return src;

    cv::Mat bgra = qimageToBgra(src.image());
    std::vector<cv::Mat> channels;
    cv::split(bgra, channels); // B, G, R, A
    cv::Mat alpha = channels[3];
    cv::Mat bgr;
    cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, bgr);

    cv::Mat smoothed = bgr;
    if (strength > 0.0) {
        double s = std::clamp(strength, 0.0, 1.0);
        cv::Mat edgePreserved;
        cv::edgePreservingFilter(bgr, edgePreserved, cv::RECURS_FILTER, 50.0f, 0.4f);
        cv::addWeighted(bgr, 1.0 - s, edgePreserved, s, 0.0, smoothed);
    }

    std::vector<cv::Mat> smoothedChannels;
    cv::split(smoothed, smoothedChannels);
    smoothedChannels.push_back(alpha);
    cv::Mat smoothedBgra;
    cv::merge(smoothedChannels, smoothedBgra);

    return unsharpMask(ImageBuffer(bgraToQImage(smoothedBgra)), amount, radiusPx, 0);
}

} // namespace CvSharpen
