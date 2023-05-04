#include <common.h>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace amr {
class ProfileReader {
 public:
  explicit ProfileReader(const char* prof_csv_path)
      : csv_path_(prof_csv_path),
        csv_fd_(nullptr),
        ts_(-1),
        eof_(false),
        prev_ts_(-1),
        prev_bid_(-1),
        prev_time_(-1),
        first_read_(true),
        prev_set_(false) {
    Reset();
  }

  ProfileReader(const ProfileReader& other) = delete;

  /* Following two snippets are to support move semantics
   * for classes with non-RAII resources (in this case, FILE* ptrs)
   * Another way to do this is to wrap the FILE* in a RAII class
   * or a unique pointer
   */
  ProfileReader(ProfileReader&& rhs) noexcept
      : csv_path_(rhs.csv_path_),
        csv_fd_(rhs.csv_fd_),
        ts_(rhs.ts_),
        eof_(rhs.eof_),
        prev_ts_(rhs.prev_ts_),
        prev_sub_ts_(rhs.prev_sub_ts_),
        prev_bid_(rhs.prev_bid_),
        prev_time_(rhs.prev_time_),
        first_read_(rhs.first_read_),
        prev_set_(rhs.prev_set_) {
    if (this != &rhs) {
      rhs.csv_fd_ = nullptr;
    }
  }

  ProfileReader& operator=(ProfileReader&& rhs) = delete;

  ~ProfileReader() { SafeCloseFile(); }

  int ReadNextTimestep(std::vector<int>& times) {
    if (eof_) return -1;

    int nlines_read = 0;
    int nblocks = 0;

    while (nlines_read == 0) {
      nblocks = ReadTimestep(ts_, times, nlines_read);
      ts_++;
    }

    logf(LOG_DBUG, "[ProfReader] Timestep: %d, lines read: %d", ts_, nlines_read);

    return nblocks;
  }

  void Reset() {
    logf(LOG_DBG2, "[ProfReader] Reset: %s", csv_path_.c_str());
    SafeCloseFile();

    csv_fd_ = fopen(csv_path_.c_str(), "r");

    if (csv_fd_ == nullptr) {
      logf(LOG_ERRO, "[ProfReader] Unable to open: %s", csv_path_.c_str());
      ABORT("Unable to open specified CSV");
    }

    ts_ = -1;
    eof_ = false;
    prev_ts_ = prev_bid_ = prev_time_ = -1;
  }

 private:
  void ReadLine(char* buf, int max_sz) {
    char* ret = fgets(buf, max_sz, csv_fd_);
    int nbread = strlen(ret);
    if (ret[nbread - 1] != '\n') {
      ABORT("buffer too small for line");
    }

    logf(LOG_DBG2, "Line read: %s", buf);
  }

  void ReadHeader() {
    char header[1024];
    ReadLine(header, 1024);
  }

 public:
  /* Caller must zero the vector if needed!!
   * Returns: Number of blocks in current ts
   * (assuming contiguous bid allocation)
   */
  int ReadTimestep(int ts_to_read, std::vector<int>& times, int& nlines_read) {
    if (eof_) return -1;

    if (first_read_) {
      ReadHeader();
      first_read_ = false;
    }

    int ts, sub_ts, rank, bid, time_us;
    sub_ts = ts_to_read;

    int max_bid = 0;

    if (prev_set_) {
      prev_set_ = false;
      if (prev_sub_ts_ == ts_to_read) {
        LogTime(times, prev_bid_, prev_time_);
        nlines_read++;

        max_bid = std::max(max_bid, prev_bid_);
      } else if (prev_sub_ts_ < ts_to_read) {
        logf(LOG_WARN, "Somehow skipped ts %d data. Dropping...", prev_sub_ts_);
      } else if (prev_sub_ts_ > ts_to_read) {
        // Wait for ts to catch up
        return max_bid;
      }
    }

    while (true) {
      int nread;
      if ((nread = fscanf(csv_fd_, "%d,%d,%d,%d,%d", &ts, &sub_ts, &rank, &bid,
                          &time_us)) == EOF) {
        eof_ = true;
        break;
      }

      if (sub_ts > ts_to_read) {
        prev_ts_ = ts;
        prev_sub_ts_ = sub_ts;
        prev_bid_ = bid;
        prev_time_ = time_us;
        prev_set_ = true;
        break;
      }

      max_bid = std::max(max_bid, bid);
      LogTime(times, bid, time_us);
      nlines_read++;
    }

    logf(LOG_DBG2, "[PolicySim] Times: %s", SerializeVector(times, 10).c_str());

    return max_bid + 1;
  }

  static void LogTime(std::vector<int>& times, int bid, int time_us) {
    if (times.size() <= bid) {
      times.resize(bid + 1, 0);
    }

    times[bid] += time_us;
  }

  void SafeCloseFile() {
    if (csv_fd_) {
      fclose(csv_fd_);
      csv_fd_ = nullptr;
    }
  }

  const std::string csv_path_;
  FILE* csv_fd_;
  int ts_;
  bool eof_;

  int prev_ts_;
  int prev_sub_ts_;
  int prev_bid_;
  int prev_time_;

  bool first_read_;
  bool prev_set_;
};
}  // namespace amr
