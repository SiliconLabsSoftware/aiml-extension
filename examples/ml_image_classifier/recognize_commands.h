/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   This file has been modified by Silicon Labs.
   ==============================================================================*/

#ifndef MODEL_RECOGNIZE_COMMANDS_H_
#define MODEL_RECOGNIZE_COMMANDS_H_

#include <cstdint>

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/tflite_bridge/micro_error_reporter.h"
#include "config/image_classifier_config.h"
#include "image_classifier.h"

// Partial implementation of std::dequeue, just providing the functionality
// that's needed to keep a record of previous neural network results over a
// short time period, so they can be averaged together to produce a more
// accurate overall prediction. This doesn't use any dynamic memory allocation
// so it's a better fit for microcontroller applications, but this does mean
// there are hard limits on the number of results it can store.
class PreviousResultsQueue {
public:
  PreviousResultsQueue(tflite::ErrorReporter * error_reporter)
    : error_reporter_(error_reporter), front_index_(0), size_(0) {
  }

  // Data structure that holds an inference result, and the time when it
  // was recorded.
  struct Result {
    Result() : time_(0), scores() {
    }
    Result(int32_t time, uint8_t * input_scores) : time_(time) {
      for (int i = 0; i < category_count; ++i) {
        scores[i] = input_scores[i];
      }
    }
    int32_t time_;
    uint8_t scores[MAX_CATEGORY_COUNT];
  };

  int size()
  {
    return size_;
  }
  bool empty()
  {
    return size_ == 0;
  }
  Result& front()
  {
    return results_[front_index_];
  }
  Result& back()
  {
    int back_index = front_index_ + (size_ - 1);
    if (back_index >= MAX_RESULT_COUNT) {
      back_index -= MAX_RESULT_COUNT;
    }
    return results_[back_index];
  }

  void push_back(const Result& entry)
  {
    if (size() >= MAX_RESULT_COUNT) {
      TF_LITE_REPORT_ERROR(
        error_reporter_,
        "Couldn't push_back latest result, too many already!");
      return;
    }
    size_ += 1;
    back() = entry;
  }

  Result pop_front()
  {
    if (size() <= 0) {
      TF_LITE_REPORT_ERROR(error_reporter_,
                           "Couldn't pop_front result, none present!");
      return Result();
    }
    Result result = front();
    front_index_ += 1;
    if (front_index_ >= MAX_RESULT_COUNT) {
      front_index_ = 0;
    }
    size_ -= 1;
    return result;
  }

  // Most of the functions are duplicates of dequeue containers, but this
  // is a helper that makes it easy to iterate through the contents of the
  // queue.
  Result& from_front(int offset)
  {
    if ((offset < 0) || (offset >= size_)) {
      TF_LITE_REPORT_ERROR(error_reporter_,
                           "Attempt to read beyond the end of the queue!");
      offset = size_ - 1;
    }
    int index = front_index_ + offset;
    if (index >= MAX_RESULT_COUNT) {
      index -= MAX_RESULT_COUNT;
    }
    return results_[index];
  }

private:
  tflite::ErrorReporter* error_reporter_;
  Result results_[MAX_RESULT_COUNT];

  int front_index_;
  int size_;
};

// This class is designed to apply a very primitive decoding model on top of the
// instantaneous results from running an image classification model on a single
// window of samples. It applies smoothing over time so that noisy individual
// label scores are averaged, increasing the confidence that apparent matches
// are real.
// To use it, you should create a class object with the configuration you
// want, and then feed results from running a TensorFlow model into the
// processing method. The timestamp for each subsequent call should be
// increasing from the previous, since the class is designed to process a stream
// of data over time.
class RecognizeCommands {
public:
  // labels should be a list of the strings associated with each one-hot score.
  // The window duration controls the smoothing. Longer durations will give a
  // higher confidence that the results are correct, but may miss some commands.
  // The detection threshold has a similar effect, with high values increasing
  // the precision at the cost of recall. The minimum count controls how many
  // results need to be in the averaging window before it's seen as a reliable
  // average. This prevents erroneous results when the averaging window is
  // initially being populated for example. The suppression argument disables
  // further recognitions for a set time after one has been triggered, which can
  // help reduce spurious recognitions.
  explicit RecognizeCommands(tflite::ErrorReporter* error_reporter,
                             int32_t average_window_duration_ms = 1000,
                             uint8_t detection_threshold = 50,
                             int32_t suppression_ms = 1500,
                             int32_t minimum_count = 3,
                             bool ignore_underscore = true);

  // Call this with the results of running a model on sample data.
  TfLiteStatus ProcessLatestResults(const TfLiteTensor* latest_results,
                                    const int32_t current_time_ms,
                                    uint8_t* found_command_index, uint8_t* score,
                                    bool* is_new_command);

private:
  // Configuration
  tflite::ErrorReporter* error_reporter_;
  int32_t average_window_duration_ms_;
  uint8_t detection_threshold_;
  int32_t suppression_ms_;
  int32_t minimum_count_;
  bool ignore_underscore_;

  // Working variables
  PreviousResultsQueue previous_results_;
  uint8_t previous_top_label_index_;
  int32_t previous_top_label_time_;
};

#endif  // MODEL_RECOGNIZE_COMMANDS_H_
