// Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "triton/common/table_printer.h"

#ifdef _WIN32
// suppress the min and max definitions in Windef.h.
#define NOMINMAX
#include <Windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace triton { namespace common {

//
// ASCII table printer.
//
void
TablePrinter::InsertRow(const std::vector<std::string>& row)
{
  std::vector<std::vector<std::string>> table_row;

  // Number of lines in each field in the record
  size_t max_height = 0;

  // Update max length of data items in each row
  for (size_t i = 0; i < row.size(); ++i) {
    table_row.push_back(std::vector<std::string>{});
    std::stringstream ss(row[i]);
    std::string line;

    size_t max_width = 0;
    while (std::getline(ss, line, '\n')) {
      table_row[i].push_back(line);
      if (line.size() > max_width)
        max_width = line.size();
    }

    if (max_width > max_widths_[i])
      max_widths_[i] = max_width;

    size_t number_of_lines = table_row[i].size();
    if (max_height < number_of_lines)
      max_height = number_of_lines;
  }

  max_heights_.push_back(max_height);
  data_.emplace_back(table_row);
}

void
TablePrinter::FairShare()
{
  // initialize original index locations
  size_t array_size = max_widths_.size();
  std::vector<size_t> idx(array_size);
  iota(idx.begin(), idx.end(), 0);

  stable_sort(idx.begin(), idx.end(), [this](size_t i1, size_t i2) {
    return this->max_widths_[i1] < this->max_widths_[i2];
  });

  size_t loop_index = 1;
  for (auto itr = idx.begin(); itr != idx.end(); ++itr) {
    // If a column is not using all the space allocated to it
    if (max_widths_[*itr] < shares_[*itr]) {
      float excess = shares_[*itr] - max_widths_[*itr];
      shares_[*itr] -= excess;

      if (itr == idx.end() - 1)
        break;
      auto update_itr = idx.begin() + (itr - idx.begin() + 1);

      // excess amount of unused space that must be distributed evenly to the
      // next columns
      float excess_per_column = excess / (array_size - loop_index);

      for (; update_itr != idx.end(); ++update_itr) {
        shares_[*update_itr] += excess_per_column;
        excess -= excess_per_column;
      }
    }
    ++loop_index;
  }

  // Remove any decimal shares
  for (auto itr = idx.begin(); itr != idx.end(); ++itr) {
    shares_[*itr] = (size_t)shares_[*itr];
  }

  // For each record
  for (size_t i = 0; i < data_.size(); i++) {
    auto current_row = data_[i];

    // For each field in the record
    for (size_t j = 0; j < current_row.size(); j++) {
      // For each line in the record
      for (size_t line_index = 0; line_index < current_row[j].size();
           line_index++) {
        std::string line = current_row[j][line_index];
        size_t num_rows = (line.size() + shares_[j] - 1) / shares_[j];

        // If the number of rows required for this record is larger than 1, we
        // will break that line and put it in multiple lines
        if (num_rows > 1) {
          // Remove the multi-line field, it will be replaced by the line
          // that can fits the column size
          data_[i][j].erase(data_[i][j].begin() + line_index);
          for (size_t k = 0; k < num_rows; k++) {
            size_t start_index =
                std::min((size_t)(k * shares_[j]), line.size());
            size_t end_index =
                std::min((size_t)((k + 1) * shares_[j]), line.size());
            data_[i][j].insert(
                data_[i][j].begin() + line_index + k,
                line.substr(start_index, end_index - start_index));
          }

          // We need to advance the index for the splitted lines.
          line_index += num_rows - 1;
        }

        if (max_heights_[i] < (num_rows - 1 + current_row[j].size()))
          max_heights_[i] += num_rows - 1;
      }
    }
  }
}

void
TablePrinter::AddRow(std::stringstream& table, size_t row_index)
{
  auto row = data_[row_index];
  size_t max_height = max_heights_[row_index];

  for (size_t j = 0; j < max_height; j++) {
    table << "|" << std::left;

    for (size_t i = 0; i < row.size(); i++) {
      if (j < row[i].size())
        table << " " << std::setw(shares_[i]) << row[i][j] << " |";
      else
        table << " " << std::setw(shares_[i]) << " "
              << " |";
    }

    // Do not add new line if this is the last row of this record
    if (j != max_height - 1)
      table << "\n";
  }
  table << "\n";
}

void
TablePrinter::AddRowDivider(std::stringstream& table)
{
  table << "+";
  for (const auto& share : shares_) {
    for (size_t i = 0; i < share + 2; i++) table << "-";
    table << "+";
  }
  table << "\n";
}

std::string
TablePrinter::PrintTable()
{
  std::stringstream table;
  table << "\n";

  FairShare();

  AddRowDivider(table);
  // Add table headers
  AddRow(table, 0);
  AddRowDivider(table);

  for (size_t j = 1; j < data_.size(); j++) {
    AddRow(table, j);
  }

  AddRowDivider(table);

  return table.str();
}

// TablePrinter will take the ownership of `headers`.
TablePrinter::TablePrinter(const std::vector<std::string>& headers)
{
  // terminal size
  size_t column_size = 500;
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  int ret = GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  if (ret && (csbi.dwSize.X != 0)) {
    column_size = csbi.dwSize.X;
  }
#else
  struct winsize terminal_size;
  int status = ioctl(STDOUT_FILENO, TIOCGWINSZ, &terminal_size);
  if ((status == 0) && (terminal_size.ws_col != 0)) {
    column_size = terminal_size.ws_col;
  }
#endif

  for (size_t i = 0; i < headers.size(); ++i) {
    max_widths_.emplace_back(0);
  }

  // Calculate fair share of every column
  size_t number_of_columns = headers.size();

  // Terminal width is the actual terminal width minus two times spaces
  // required before and after each column and number of columns plus 1 for
  // the pipes between the columns
  size_t terminal_width =
      column_size - (2 * number_of_columns) - (number_of_columns + 1);
  int equal_share = terminal_width / headers.size();

  for (size_t i = 0; i < headers.size(); ++i) {
    shares_.emplace_back(equal_share);
    terminal_width -= equal_share;
  }

  InsertRow(headers);
}

}}  // namespace triton::common
