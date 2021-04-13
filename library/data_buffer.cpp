#include "settings.h"

#include <stdexcept>

#include "fastcgi2/data_buffer.h"
#include "fastcgi2/util.h"

#include "details/data_buffer_impl.h"
#include "details/string_buffer.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace fastcgi
{

DataBuffer::DataBuffer() : begin_(0), end_(0)
{}

DataBuffer::DataBuffer(DataBuffer buffer, std::uint64_t begin, std::uint64_t end) :
	data_(buffer.data_), begin_(begin), end_(end)
{
	if (end_ > data_->size()) {
		throw std::out_of_range("Incorrect index");
	}
}

DataBuffer
DataBuffer::create(const char *data, std::uint64_t size) {
	return create(new StringBuffer(data, size));
}

DataBuffer
DataBuffer::create(DataBufferImpl *impl) {
	DataBuffer result;
	result.data_.reset(impl);
	result.end_ = result.data_->size();
	return result;
}

void
DataBuffer::checkIndex(std::uint64_t index) const {
	if (0 == index && 0 == begin_ && 0 == end_) {
		return;
	}
	if (index < begin_ || index >= end_) {
		throw std::out_of_range("Incorrect index");
	}
}

std::uint64_t
DataBuffer::beginIndex() const {
	return begin_;
}

std::uint64_t
DataBuffer::read(std::uint64_t pos, char *data, std::uint64_t len) {
	if (pos >= end_ - begin_) {
		return 0;
	}
	return data_->read(pos + begin_, data, std::min(end_ - begin_ - pos, len));
}

std::uint64_t
DataBuffer::write(std::uint64_t pos, const char *data, std::uint64_t len) {
	if (pos >= end_ - begin_) {
		return 0;
	}
	return data_->write(pos + begin_, data, std::min(end_ - begin_ - pos, len));
}

std::uint64_t
DataBuffer::endIndex() const {
	return end_;
}

bool
DataBuffer::isNil() const {
	return NULL == data_.get();
}

bool
DataBuffer::empty() const {
	return isNil() || begin_ == end_;
}

std::uint64_t
DataBuffer::size() const {
	return end_ - begin_;
}

void
DataBuffer::resize(std::uint64_t size) {
	data_->resize(begin_ + size);
	end_ = begin_ + size;
}

DataBufferImpl*
DataBuffer::impl() const {
	return data_.get();
}

char
DataBuffer::at(std::uint64_t pos) const {
	checkIndex(begin_ + pos);
	return data_->at(begin_ + pos);
}

std::uint64_t
DataBuffer::find(std::uint64_t pos, const char* buf, std::uint64_t len) const {
	checkIndex(pos);
	return data_->find(pos, end_, buf, len) - begin_;
}

DataBuffer
DataBuffer::trim() const {
	if (empty()) {
		return *this;
	}
	std::pair<std::uint64_t, std::uint64_t> res = data_->trim(begin_, end_);
	DataBuffer res_buffer = *this;
	res_buffer.begin_ = res.first;
	res_buffer.end_ = res.second;
	return res_buffer;
}

DataBuffer
DataBuffer::trimn(std::uint64_t b, std::uint64_t e) const {
	if (empty()) {
		return *this;
	}
	DataBuffer res_buffer = *this;
	res_buffer.begin_ += b;
	if (res_buffer.begin_ >= res_buffer.end_) {
		res_buffer.begin_ = res_buffer.end_;
		return res_buffer;
	}
	res_buffer.end_ = std::max(res_buffer.end_ - e, res_buffer.begin_);
	return res_buffer;
}

bool
DataBuffer::split(const std::string &delim, DataBuffer &first, DataBuffer &second) const {
	first = *this;
	second = DataBuffer();
	second.data_ = data_;

	if (empty()) {
		return false;
	}

	std::uint64_t res = find(begin_, delim.c_str(), delim.size());
	if (res == size()) {
		return false;
	}
	first.begin_ = begin_;
	first.end_ = first.begin_ + res;
	second.begin_ = first.end_ + delim.size();
	second.end_ = end_;
	return true;
}

bool
DataBuffer::split(char delim, DataBuffer &first, DataBuffer &second) const {
	first = *this;
	second = DataBuffer();
	second.data_ = data_;
	if (empty()) {
		return false;
	}
	std::uint64_t res = find(begin_, &delim, 1);
	if (res == size()) {
		return false;
	}
	first.begin_ = begin_;
	first.end_ = first.begin_ + res;
	second.begin_ = first.end_ + 1;
	second.end_ = end_;
	return true;
}

bool
DataBuffer::startsWith(const std::string &data) const {
	std::uint64_t data_size = data.size();
	if (data_size > size()) {
		return false;
	}
	for (std::uint64_t i = 0; i < data_size; ++i) {
		if (data[i] != at(i)) {
			return false;
		}
	}
	return true;
}

bool
DataBuffer::startsWithCI(const std::string &data) const {
	std::uint64_t data_size = data.size();
	if (data_size > size()) {
		return false;
	}
	for (std::uint64_t i = 0; i < data_size; ++i) {
		if (tolower(data[i]) != tolower(at(i))) {
			return false;
		}
	}
	return true;
}

bool
DataBuffer::endsWith(const std::string &data) const {
	std::uint64_t data_size = data.size();
	std::uint64_t sz = size();
	if (data_size > sz) {
		return false;
	}
	for (std::uint64_t i = 1; i <= data_size; ++i) {
		if (data[data_size - i] != at(sz - i)) {
			return false;
		}
	}
	return true;
}

bool
DataBuffer::endsWithCI(const std::string &data) const {
	std::uint64_t data_size = data.size();
	std::uint64_t sz = size();
	if (data_size > sz) {
		return false;
	}
	for (std::uint64_t i = 1; i <= data_size; ++i) {
		if (tolower(data[data_size - i]) != tolower(at(sz - i))) {
			return false;
		}
	}
	return true;
}

DataBuffer::SegmentIterator
DataBuffer::begin() const {
	return SegmentIterator(*this);
}

DataBuffer::SegmentIterator
DataBuffer::end() const {
	return SegmentIterator();
}

void
DataBuffer::toString(std::string &str) const {
	str.clear();
	str.reserve(size());
	for (SegmentIterator it = begin(), end; it != end; ++it) {
		std::pair<char*, std::uint64_t> chunk = *it;
		str.append(chunk.first, chunk.second);
	}
}

DataBuffer::SegmentIterator::SegmentIterator() :
	pos_begin_(0), pos_end_(0), data_(std::pair<char*, std::uint64_t>(NULL, 0))
{}

DataBuffer::SegmentIterator::SegmentIterator(const DataBuffer &buffer) :
	buffer_(buffer), pos_begin_(buffer.begin_)
{
	if (buffer_.empty()) {
		buffer_ = DataBuffer();
		pos_begin_ = 0;
		pos_end_ = 0;
		return;
	}
	buffer_.data_ = std::shared_ptr<DataBufferImpl>(buffer.data_->getCopy());
	std::pair<std::uint64_t, std::uint64_t> segment = buffer_.data_->segment(pos_begin_);
	pos_end_ = std::min(segment.second, buffer_.end_);
}

DataBufferImpl*
DataBuffer::SegmentIterator::impl() const {
	return buffer_.data_.get();
}

std::pair<char*, std::uint64_t>
DataBuffer::SegmentIterator::operator*() const {
	if (*this == SegmentIterator()) {
		return std::pair<char*, std::uint64_t>(NULL, 0);
	}
	std::pair<char*, std::uint64_t> res = buffer_.data_->chunk(pos_begin_);
	res.second = std::min(res.second, buffer_.end_ - pos_begin_);
	return res;
}

std::pair<char*, std::uint64_t>*
DataBuffer::SegmentIterator::operator->() const {
	data_ = operator*();
	return &data_;
}

DataBuffer::SegmentIterator&
DataBuffer::SegmentIterator::operator++() {
	if (*this == SegmentIterator()) {
		return *this;
	}

	if (pos_end_ >= buffer_.end_) {
		buffer_ = DataBuffer();
		pos_begin_ = 0;
		pos_end_ = 0;
		return *this;
	}

	std::pair<std::uint64_t, std::uint64_t> segment = buffer_.data_->segment(pos_end_);
	pos_begin_ = pos_end_;
	pos_end_ = std::min(segment.second, buffer_.end_);
	return *this;
}

DataBuffer::SegmentIterator
DataBuffer::SegmentIterator::operator++(int) {
	SegmentIterator iter = *this;
	return ++iter;
}

bool
operator==(const DataBuffer::SegmentIterator &lhs, const DataBuffer::SegmentIterator &rhs) {
	return lhs.impl() == rhs.impl() &&
		lhs.pos_begin_ == rhs.pos_begin_ &&
		lhs.pos_end_ == rhs.pos_end_;
}

bool
operator!=(const DataBuffer::SegmentIterator &lhs, const DataBuffer::SegmentIterator &rhs) {
	return !(lhs == rhs);
}

} // namespace fastcgi
