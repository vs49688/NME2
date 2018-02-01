#pragma once

#include "CripackReader.h"

#include <qwidget.h>
#include <qgridlayout.h>
#include <qvideowidget.h>
#include <qmediaplayer.h>
#include <qprocess.h>
#include <qeventloop.h>
#include <qlabel.h>
#include <qmediaplaylist.h>

class USMPlayer : public QWidget, public CripackReader {
    Q_OBJECT

    public:
    explicit USMPlayer(std::string fpath, std::map<uint32_t, QIcon>& icons, QWidget* parent = nullptr);

    ~USMPlayer() {

    }

    private:
    std::ifstream infile;
    uint64_t fsize;
    QByteArray video;

    std::string infile_path;

    QGridLayout* layout;
    QVideoWidget* video_widget;
    QMediaPlayer* player;

    QBuffer* analyse();

    struct StreamInfo {
        std::string fname;
        uint64_t fsize;
        uint64_t dsize;

        uint32_t stmid;
        uint16_t chno;
        uint16_t minchk;
        uint32_t minbuf;
        uint32_t avbps;

        uint64_t bytes_read;
        uint64_t payload_bytes;
        uint32_t alive;
    };
};
