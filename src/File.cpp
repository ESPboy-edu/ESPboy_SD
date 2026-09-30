/*
  SD - File implementation for ESPboy_SDlib
*/

#include "ESPboy_SD.h"

namespace ESPboySDLib {

File::File(SdFile f, const char *n) {
  _file = (SdFile *)malloc(sizeof(SdFile));
  if (_file) {
    memcpy(_file, &f, sizeof(SdFile));
    strncpy(_name, n, 12);
    _name[12] = 0;
  }
}

File::File(void) {
  _file = 0;
  _name[0] = 0;
}

char *File::name(void) {
  return _name;
}

bool File::isDirectory(void) {
  return (_file && _file->isDir());
}

size_t File::write(uint8_t val) {
  return write(&val, 1);
}

size_t File::write(const uint8_t *buf, size_t size) {
  size_t t;
  if (!_file) {
    setWriteError();
    return 0;
  }
  _file->clearWriteError();
  t = _file->write(buf, size);
  if (_file->getWriteError()) {
    setWriteError();
    return 0;
  }
  return t;
}

int File::availableForWrite() {
  if (_file) {
    return _file->availableForWrite();
  }
  return 0;
}

int File::peek() {
  if (! _file) return 0;
  int c = _file->read();
  if (c != -1) {
    _file->seekCur(-1);
  }
  return c;
}

int File::read() {
  if (_file) return _file->read();
  return -1;
}

int File::read(void *buf, uint16_t nbyte) {
  if (_file) return _file->read(buf, nbyte);
  return 0;
}

int File::available() {
  if (! _file) return 0;
  uint32_t n = size() - position();
  return n > 0X7FFF ? 0X7FFF : n;
}

void File::flush() {
  if (_file) _file->sync();
}

bool File::seek(uint32_t pos) {
  if (! _file) return false;
  return _file->seekSet(pos);
}

uint32_t File::position() {
  if (! _file) return -1;
  return _file->curPosition();
}

uint32_t File::size() {
  if (! _file) return 0;
  return _file->fileSize();
}

void File::close() {
  if (_file) {
    _file->close();
    free(_file);
    _file = 0;
  }
}

File::operator bool() {
  if (_file) return _file->isOpen();
  return false;
}

File File::openNextFile(uint8_t mode) {
  dir_t p;
  while (_file->readDir(&p) > 0) {
    if (p.name[0] == DIR_NAME_FREE) return File();
    if (p.name[0] == DIR_NAME_DELETED || p.name[0] == '.') continue;
    if (!DIR_IS_FILE_OR_SUBDIR(&p)) continue;

    SdFile f;
    char name[13];
    _file->dirName(p, name);

    if (f.open(_file, name, mode)) {
      return File(f, name);
    } else {
      return File();
    }
  }
  return File();
}

void File::rewindDirectory(void) {
  if (isDirectory()) {
    _file->rewind();
  }
}

} // namespace ESPboySDLib