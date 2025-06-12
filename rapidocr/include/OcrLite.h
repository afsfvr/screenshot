#ifndef __OCR_LITE_H__
#define __OCR_LITE_H__

#include <string>

struct Point {
    Point() {}
    Point(int x, int y): x(x), y(y) {}
    int x;
    int y;
};

struct BGRImage {
    int width;
    int height;
    std::vector<unsigned char> data;
};

struct TextBlock {
    std::vector<Point> boxPoint;
    float boxScore;
    int angleIndex;
    float angleScore;
    double angleTime;
    std::string text;
    std::vector<float> charScores;
    double crnnTime;
    double blockTime;
};

struct OcrResult {
    double dbNetTime;
    std::vector<TextBlock> textBlocks;
    BGRImage boxImg;
    double detectTime;
    std::string strRes;
};

class OcrLiteImpl;

class OcrLite{
public:
    OcrLite();

    ~OcrLite();

    void setNumThread(int numOfThread);

    void initLogger(bool isConsole, bool isPartImg, bool isResultImg);

    void enableResultTxt(const char *path, const char *imgName);

    void setGpuIndex(int gpuIndex);

    bool initModels(const std::string &detPath, const std::string &clsPath,
                    const std::string &recPath, const std::string &keysPath);

    void Logger(const char *format, ...);

    OcrResult detect(const char *path, const char *imgName,
                     int padding, int maxSideLen,
                     float boxScoreThresh, float boxThresh, float unClipRatio, bool doAngle, bool mostAngle);

    OcrResult detectImageBytes(const uint8_t *data, long dataLength, int grey, int padding, int maxSideLen,
                               float boxScoreThresh, float boxThresh, float unClipRatio, bool doAngle, bool mostAngle);

    OcrResult detectBitmap(uint8_t *bitmapData, int width, int height,int channels, int padding, int maxSideLen,
                           float boxScoreThresh, float boxThresh, float unClipRatio, bool doAngle, bool mostAngle);

private:
    OcrLiteImpl* pImpl;
};

#endif //__OCR_LITE_H__
