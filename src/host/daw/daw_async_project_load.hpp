#pragma once
#include <archive.h>
#include <archive_entry.h>
#include <cstddef>
#include <memory>
#include <map>
#include <utility>
#include "assert_dbg.h"
#include "cursor.hpp"
#include "daw_async_task.hpp"
#include "file/projectfile.hpp"
#include "host/audiocache/audiocache.hpp"
#include "host/audiocache/audiocache.hpp"
#include "host/audiocache/samplefileidx.hpp"
#include "host/clip/clip.hpp"
#include "host/daw/mainctrl.hpp"
#include "host/host.hpp"
#include "logging.hpp"
#include "str_util.hpp"
#include "types.hpp"
#include "window_impl.hpp"
#include "wave/downsample.hpp"

namespace DAW {

struct samplefile_index_incremental_loader_t {
    audiocache* cache = nullptr;
    struct archive* ar = nullptr;
    samplefile_index_t index;
    size_t indexSize;
    size_t indexPos = 0;
    String workingDir;
    bool bFinished = false;
    struct archive_entry* entry = nullptr;
    String curFileName;
    std::shared_ptr<audiocache::fileloader> loader;
    samplefile_index_incremental_loader_t(audiocache* cache, struct archive* ar, samplefile_index_t& index, String workingDir)
        : cache(cache),
        ar(ar),
        index(index),
        indexSize(index.list.size()),
        workingDir(std::move(workingDir)) {
    }
    ~samplefile_index_incremental_loader_t() {
        if (entry && !bFinished) {
            archive_entry_free(entry);
        }
        if (ar && !bFinished) {
            archive_read_free(ar);
        }
    }
    double getProgress() const {
        auto progress = double(!indexSize ? 1.0 : indexPos / double(indexSize));
        if (loader) {
            progress += loader->getProgress() / indexSize;
        }
        return progress;
    }
    void step() {
        if (ar) {
            entry = nullptr;
            if (archive_read_next_header(ar, &entry) == ARCHIVE_OK) {
                curFileName = archive_entry_pathname(entry);
            } else {
                entry = nullptr;
                bFinished = true;
                archive_read_free(ar);
            }
        } else {
            if (indexPos < index.list.size()) {
                curFileName = index.list[indexPos].name;
            } else {
                bFinished = true;
            }
        }
    }

    void openFile(int32_t id) {
        auto loader = std::make_shared<audiocache::fileloader>();
        loader->setTargetSampleRate(cache->getSampleRate());
        if (!loader->resolveFile(curFileName, workingDir, ar == nullptr, id)) {
            log_lf(Log::L_ERROR, "Failed to resolve file %s: %s\n", curFileName.c_str(), loader->getError().c_str());
            loader->getFile()->state |= audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_MISSING;
        } else if (!loader->preloadFile(ar, entry)) {
            log_lf(Log::L_ERROR, "Failed to preload file %s: %s\n", curFileName.c_str(), loader->getError().c_str());
            loader->getFile()->state |= audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_MISSING;
        }
        auto spFile = loader->getSPFile();
        if (spFile) {
            cache->addFile(spFile);
        }
        if (loader->isOk() && !loader->isFinished()) {
            this->loader = std::move(loader);
        }
    }
    
    void loadSingleStep() {
        if (bFinished) {
            return;
        }
        if (curFileName.empty()) {
            step();
        }
        if (bFinished) {
            return;
        }
        if (!loader) {
            if (ar) {
                auto it = index.list.begin();
                const auto itEnd = index.list.end();
                for (; it != itEnd; ++it) {
                    if (it->name == curFileName) {
                        openFile(it->id);
                        it = index.list.erase(it);
                        break;
                    }
                }
            } else {
                openFile(index.list[indexPos].id);
            }
        }
        if (loader) {
            if (!loader->isFinished()) {
                if (loader->loadFileIncremental()) {
                    return;
                }
            } else {
                if (!loader->isOk()) {
                    log_lf(Log::L_ERROR, "Failed to load file %s: %s\n", curFileName.c_str(), loader->getError().c_str());
                }
                loader.reset();
            }
        }
        if (!loader) {
            curFileName = "";
            indexPos++;
        }
    }
    bool isFinished() const {
        return bFinished;
    }
};

struct load_project_task final : public async_task_t {
    DawInstance* daw;
    std::shared_ptr<project_to_load_t> projectToLoad;
    int32_t step = 0;
    int32_t substep = 0;
    int32_t numSubsteps = 0;
    String taskDesc;
    String progressDesc;
    bool projectLoadErrored = false;
    std::vector<effectbase*> pluginsDeferred; 
    std::shared_ptr<DAW::samplefile_index_incremental_loader_t> sampleLoader;
    load_project_task(DawInstance* daw, std::shared_ptr<project_to_load_t>&& projectToLoad)
        : daw(daw)
        , projectToLoad(projectToLoad) {
        String projectFileName;
        SplitPath(projectToLoad->projectfile->path, nullptr, &projectFileName, nullptr);
        taskDesc = "Load " + projectFileName;
        progressDesc = "";
    }
    String getTaskName() const override {
        return taskDesc;
    }
    String getProgressDesc() const override {
        return progressDesc;
    }
    void run() override;
    void getPreciseProgress(double& progressOverall, double& progressDetail) override;
};

} // namespace DAW
