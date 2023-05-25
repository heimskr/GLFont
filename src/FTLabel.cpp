
#include "FTLabel.h"
#include "GLUtils.h"
#include "FontAtlas.h"

#include <stdio.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>

// GLM
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/transform.hpp"

FTLabel::FTLabel(std::shared_ptr<GLFont> ftFace, int windowWidth, int windowHeight) :
_uniformMVPHandle(-1),
_text(""),
_x(0),
_y(0),
_width(0),
_height(0),
_textColor(0, 0, 0, 1),
_alignment(FontFlags::LeftAligned),
_indentationPix(0),
_isInitialized(false) {
	CHECKGL

	setFont(ftFace);
	setWindowSize(windowWidth, windowHeight);
	CHECKGL

	// Intially enabled flags
	_flags = FontFlags::LeftAligned | FontFlags::WordWrap;

	// Since we are dealing with 2D text, we will use an orthographic projection matrix
	_projection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, // View: left, right, bottom, top (we are using normalized coords)
							 0.1f,                     // Near clipping distance
							 100.0f);                  // Far clipping distance

	_view = glm::lookAt(glm::vec3(0, 0, 1),  // Camera position in world space (doesn't really apply when using an ortho projection matrix)
						glm::vec3(0, 0, 0),  // look at origin
						glm::vec3(0, 1, 0)); // Head is up (set to 0, -1, 0 to look upside down)

	_model = glm::mat4(1.0f); // Identity matrix

	recalculateMVP();

	CHECKGL
	// Load the shaders
	_programId = glCreateProgram();
	CHECKGL
	GLUtils::loadShader("resources/fontVertex.shader", GL_VERTEX_SHADER, _programId);
	CHECKGL
	GLUtils::loadShader("resources/fontFragment.shader", GL_FRAGMENT_SHADER, _programId);
	CHECKGL

	glUseProgram(_programId);
	CHECKGL

	// Create and bind the vertex array object
	glGenVertexArrays(1, &_vao);
	CHECKGL
	glBindVertexArray(_vao);
	CHECKGL

	// Set default pixel size and create the texture
	setPixelSize(48); // default pixel size
	CHECKGL

	// Get shader handles
	_uniformTextureHandle = glGetUniformLocation(_programId, "tex");
	CHECKGL
	_uniformTextColorHandle = glGetUniformLocation(_programId, "textColor");
	CHECKGL
	_uniformMVPHandle = glGetUniformLocation(_programId, "mvp");
	CHECKGL

	GLuint curTex = _fontAtlas[_pixelSize]->getTexId(); // get texture ID for this pixel size
	CHECKGL

	glActiveTexture(GL_TEXTURE0 + curTex);
	CHECKGL
	glBindTexture(GL_TEXTURE_2D, curTex);
	CHECKGL

	// Set our uniforms
	glUniform1i(_uniformTextureHandle, curTex);
	CHECKGL
	glUniform4fv(_uniformTextColorHandle, 1, glm::value_ptr(_textColor));
	CHECKGL
	glUniformMatrix4fv(_uniformMVPHandle, 1, GL_FALSE, glm::value_ptr(_mvp));
	CHECKGL

	// Create the vertex buffer object
	glGenBuffers(1, &_vbo);
	CHECKGL
	glBindBuffer(GL_ARRAY_BUFFER, _vbo);
	CHECKGL

	glUseProgram(0);
	CHECKGL

	_isInitialized = true;
}

FTLabel::FTLabel(GLFont* ftFace, int windowWidth, int windowHeight) :
  FTLabel(std::make_shared<GLFont>(*ftFace), windowWidth, windowHeight)
{}

FTLabel::FTLabel(std::shared_ptr<GLFont> ftFace, const char *text, float x, float y, int width, int height, int windowWidth, int windowHeight) :
  FTLabel(ftFace, text, x, y, windowWidth, windowHeight)
{
	_width = width;
	_height = height;

	recalculateVertices(text, x, y, width, height);
}

FTLabel::FTLabel(std::shared_ptr<GLFont> ftFace, const char *text, float x, float y, int windowWidth, int windowHeight) :
FTLabel(ftFace, windowWidth, windowHeight)
{
	_text = (char*)text;
	_x = x;
	_y = y;
}

FTLabel::~FTLabel() {
	glDeleteBuffers(1, &_vbo);
	glDeleteVertexArrays(1, &_vao);
}

void FTLabel::recalculateVertices(const char *text, float x, float y, int width, int height) {
	CHECKGL
	_coords.clear(); // case there are any existing coords
	CHECKGL
	// Break the text into individual words
	std::vector<std::string> words = splitText(text);

	std::vector<std::string> lines;
	int widthRemaining = width;
	int spaceWidth = calcWidth(" ");
	int indent = (_flags & FontFlags::Indented) && _alignment != FontFlags::CenterAligned ? _pixelSize : 0;

	CHECKGL
	// Create lines from our text, each containing the maximum amount of words we can fit within the given width
	std::string curLine = "";
	for (const auto &word: words) {
		int wordWidth = calcWidth(word.c_str());

		if (wordWidth - spaceWidth > widthRemaining && width /* make sure there is a width specified */) {

			// If we have passed the given width, add this line to our collection and start a new line
			lines.push_back(curLine);
			widthRemaining = width - wordWidth;
			curLine = "";

			// Start next line with current word
			curLine.append(word.c_str());
		} else {
			// Otherwise, add this word to the current line
			curLine.append(word.c_str());
			widthRemaining = widthRemaining - wordWidth;
		}
	}

	CHECKGL
	// Add the last line to lines
	if (curLine != "")
		lines.push_back(curLine);
	CHECKGL

	// Print each line, increasing the y value as we go
	float startY = y - (_face->size->metrics.height >> 6);
	for (const auto &line: lines) {
		// If we go past the specified height, stop drawing
		if (y - startY > height && height)
			break;

		recalculateVertices(line.c_str(), x + indent, y);
		y += (_face->size->metrics.height >> 6);
		indent = 0;
	}
	CHECKGL
	glUseProgram(_programId);
	CHECKGL
	glEnable(GL_BLEND);
	CHECKGL
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	CHECKGL

	GLuint curTex = _fontAtlas[_pixelSize]->getTexId();
	glActiveTexture(GL_TEXTURE0 + curTex);
	CHECKGL

	glBindBuffer(GL_ARRAY_BUFFER, _vbo);

	CHECKGL
	glBindTexture(GL_TEXTURE_2D, curTex);
	CHECKGL
	glUniform1i(_uniformTextureHandle, curTex);
	CHECKGL

	glBindVertexArray(_vao);
	CHECKGL

	glEnableVertexAttribArray(0);
	CHECKGL

	// Send the data to the gpu
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Point), NULL);
	CHECKGL
	glBufferData(GL_ARRAY_BUFFER, _coords.size() * sizeof(Point), _coords.data(), GL_DYNAMIC_DRAW);
	CHECKGL
	glDrawArrays(GL_TRIANGLES, 0, _coords.size());
	CHECKGL
	_numVertices = _coords.size();

	glDisableVertexAttribArray(0);
	CHECKGL
	glBindTexture(GL_TEXTURE_2D, 0);
	CHECKGL
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	CHECKGL

	glDisable(GL_BLEND);
	CHECKGL
	glUseProgram(0);
	CHECKGL
}

void FTLabel::recalculateVertices(const char *text, float x, float y) {

	// Coordinates passed in should specify where to start drawing from the top left of the text,
	// but FreeType starts drawing from the bottom-right, therefore move down one line
	y += _face->size->metrics.height >> 6;

	// Calculate alignment (if applicable)
	int textWidth = calcWidth(text);
	if (_alignment == FontFlags::CenterAligned)
		x -= textWidth / 2.0;
	else if (_alignment == FontFlags::RightAligned)
		x -= textWidth;

	// Normalize window coordinates
	x = -1 + x * _sx;
	y = 1 - y * _sy;


	int atlasWidth = _fontAtlas[_pixelSize]->getAtlasWidth();
	int atlasHeight = _fontAtlas[_pixelSize]->getAtlasHeight();

	FontAtlas::Character* chars = _fontAtlas[_pixelSize]->getCharInfo();

	for (const char *p = text; *p; ++p) {
		const auto &character = chars[static_cast<size_t>(*p)];

		float x2 = x + character.bitmapLeft * _sx; // scaled x coord
		float y2 = -y - character.bitmapTop * _sy; // scaled y coord
		float w = character.bitmapWidth * _sx;     // scaled width of character
		float h = character.bitmapHeight * _sy;    // scaled height of character

		// Calculate kerning value
		FT_Vector kerning;
		FT_Get_Kerning(_face,              // font face handle
					   *p,                 // left glyph
					   *(p + 1),           // right glyph
					   FT_KERNING_DEFAULT, // kerning mode
					   &kerning);          // variable to store kerning value

		// Advance cursor to start of next character
		x += (character.advanceX + (kerning.x >> 6)) * _sx;
		y += character.advanceY * _sy;

		// Skip glyphs with no pixels (e.g. spaces)
		if (!w || !h)
			continue;


		_coords.push_back(Point(x2,                // window x
								-y2,               // window y
								character.xOffset, // texture atlas x offset
								0));               // texture atlas y offset

		_coords.push_back(Point(x2 + w,
								-y2,
								character.xOffset + character.bitmapWidth / atlasWidth,
								0));

		_coords.push_back(Point(x2,
								-y2 - h,
								character.xOffset,
								character.bitmapHeight / atlasHeight));

		_coords.push_back(Point(x2 + w,
								-y2,
								character.xOffset + character.bitmapWidth / atlasWidth,
								0));

		_coords.push_back(Point(x2,
								-y2 - h,
								character.xOffset,
								character.bitmapHeight / atlasHeight));

		_coords.push_back(Point(x2 + w,
								-y2 - h,
								character.xOffset + character.bitmapWidth / atlasWidth,
								character.bitmapHeight / atlasHeight));
	}

}

void FTLabel::render() {
	glUseProgram(_programId);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	GLuint curTex = _fontAtlas[_pixelSize]->getTexId();
	glActiveTexture(GL_TEXTURE0 + curTex);

	glBindBuffer(GL_ARRAY_BUFFER, _vbo);

	glBindTexture(GL_TEXTURE_2D, curTex);
	glUniform1i(_uniformTextureHandle, curTex);

	glEnableVertexAttribArray(0);

	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Point), 0);
	glDrawArrays(GL_TRIANGLES, 0, _numVertices);

	glDisableVertexAttribArray(0);

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glDisable(GL_BLEND);
	glUseProgram(0);
}

std::vector<std::string> FTLabel::splitText(std::string_view text) {
	std::vector<std::string> words;
	size_t startPos = 0; // start position of current word
	size_t endPos = text.find(' '); // end position of current word

	if (endPos == static_cast<size_t>(-1)) {
		// There is only one word, so return early
		words.emplace_back(text);
		return words;
	}

	// Find each word in the text (delimited by spaces) and add it to our vector of words
	while(endPos != std::string::npos) {
		words.emplace_back(text.substr(startPos, endPos  - startPos + 1));
		startPos = endPos + 1;
		endPos = text.find(' ', startPos);
	}

	// Add last word
	words.emplace_back(text.substr(startPos, std::min(endPos, text.size()) - startPos + 1));

	return words;
}

int FTLabel::calcWidth(const char *text) {
	int width = 0;
	FontAtlas::Character* chars = _fontAtlas[_pixelSize]->getCharInfo();
	for(const char* p = text; *p; ++p) {
		width += chars[static_cast<size_t>(*p)].advanceX;
	}

	return width;
}

void FTLabel::setText(const char *text) {
	_text = text;
	recalculateVertices(text, _x, _y, _width, _height);
}

void FTLabel::setText(const std::string &text) {
	_text = text;
	recalculateVertices(_text.c_str(), _x, _y, _width, _height);
}

std::string FTLabel::getText() {
	return std::string(_text);
}

void FTLabel::setPosition(float x, float y) {
	_x = x;
	_y = y;

	if (!_text.empty()) {
		recalculateVertices(_text.c_str(), _x, _y, _width, _height);
	}
}

float FTLabel::getX() {
	return _x;
}

float FTLabel::getY() {
	return _y;
}

void FTLabel::setSize(int width, int height) {
	_width = width;
	_height = height;

	if (!_text.empty()) {
		recalculateVertices(_text.c_str(), _x, _y, width, height);
	}
}

int FTLabel::getWidth() {
	return _width;
}

int FTLabel::getHeight() {
	return _height;
}

void FTLabel::setFontFlags(int flags) {
	_flags = flags;
}

void FTLabel::appendFontFlags(int flags) {
	_flags |= flags;
}

int FTLabel::getFontFlags() {
	return _flags;
}

void FTLabel::setIndentation(int pixels) {
	_indentationPix = pixels;
}

int FTLabel::getIndentation() {
	return _indentationPix;
}

void FTLabel::setColor(float r, float b, float g, float a) {
	_textColor = glm::vec4(r, b, g, a);

	// Update the textColor uniform
	if (_programId != static_cast<GLuint>(-1)) {
		glUseProgram(_programId);
		glUniform4fv(_uniformTextColorHandle, 1, glm::value_ptr(_textColor));
		glUseProgram(0);
	}
}

glm::vec4 FTLabel::getColor() {
	return _textColor;
}

void FTLabel::setFont(std::shared_ptr<GLFont> ftFace) {
	_ftFace = ftFace;
	_face = _ftFace->getFaceHandle(); // shortcut
}

char* FTLabel::getFont() {
	return _font;
}

void FTLabel::setAlignment(FTLabel::FontFlags alignment) {
	_alignment = alignment;

	if (_isInitialized) {
		recalculateVertices(_text.c_str(), _x, _y, _width, _height);
	}
}

FTLabel::FontFlags FTLabel::getAlignment() {
	return _alignment;
}

void FTLabel::calculateAlignment(const char* text, float &x) {
	if (_alignment == FTLabel::FontFlags::LeftAligned)
		return; // no need to calculate alignment

	int totalWidth = 0; // total width of the text to render in window space
	FontAtlas::Character* chars = _fontAtlas[_pixelSize]->getCharInfo();

	// Calculate total width
	for(const char* p = text; *p; ++p)
		totalWidth += chars[static_cast<size_t>(*p)].advanceX;

	if (_alignment == FTLabel::FontFlags::CenterAligned)
		x -= totalWidth / 2.0;
	else if (_alignment == FTLabel::FontFlags::RightAligned)
		x -= totalWidth;
}

void FTLabel::setPixelSize(int size) {
	_pixelSize = size;

	CHECKGL

	// Create texture atlas for characters of this pixel size if there isn't already one
	if (!_fontAtlas[_pixelSize]) {
		CHECKGL
		_fontAtlas[_pixelSize] = std::make_shared<FontAtlas>(_face, _pixelSize);
	}

	CHECKGL

	if (_isInitialized) {
		CHECKGL
		recalculateVertices(_text.c_str(), _x, _y, _width, _height);
	}
}

void FTLabel::setWindowSize(int width, int height) {
	_windowWidth = width;
	_windowHeight = height;

	// Recalculate sx and sy
	_sx = 2.0 / _windowWidth;
	_sy = 2.0 / _windowHeight;

	if (_isInitialized) {
		recalculateVertices(_text.c_str(), _x, _y, _width, _height);
	}
}

void FTLabel::rotate(float degrees, float x, float y, float z) {
	float rad = degrees * DEG_TO_RAD;
	_model = glm::rotate(_model, rad, glm::vec3(x, y, z));
	recalculateMVP();
}

void FTLabel::scale(float x, float y, float z) {
	_model = glm::scale(glm::vec3(x, y, z));
	recalculateMVP();
}

void FTLabel::recalculateMVP() {
	_mvp = _projection * _view * _model;

	if (_uniformMVPHandle != -1) {
		glUseProgram(_programId);
		glUniformMatrix4fv(_uniformMVPHandle, 1, GL_FALSE, glm::value_ptr(_mvp));
		glUseProgram(0);
	}
}