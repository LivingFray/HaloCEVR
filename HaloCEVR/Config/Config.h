#pragma once
#include <fstream>
#include <string>
#include <unordered_map>
#include <algorithm>
#include "../Maths/Vectors.h"
#include "../Logger.h"

class Property {
public:
	Property(const std::string& description) : description_(description), guid(-1) {}

	virtual ~Property() = default;

	std::string& GetDesc() { return description_; };
protected:
	std::string description_;
	int guid;
	friend class Config;
};

class BoolProperty : public Property {
public:
	BoolProperty(bool value, const std::string& description) : value_(value), default_(value), Property(description) {}
	bool Value() const { return value_; }
	bool DefaultValue() const { return default_; }
private:
	bool value_;
	bool default_;
	friend class Config;
};

class IntProperty : public Property {
public:
	IntProperty(int value, const std::string& description) : value_(value), default_(value), Property(description) {}
	int Value() const { return value_; }
	int DefaultValue() const { return default_; }
private:
	int value_;
	int default_;
	friend class Config;
};

class FloatProperty : public Property {
public:
	FloatProperty(float value, const std::string& description) : value_(value), default_(value), Property(description) {}
	float Value() const { return value_; }
	float DefaultValue() const { return default_; }
private:
	float value_;
	float default_;
	friend class Config;
};

class StringProperty : public Property {
public:
	StringProperty(const std::string& value, const std::string& description) : value_(value), default_(value), Property(description) {}
	std::string Value() const { return value_; }
	std::string DefaultValue() const { return default_; }
private:
	std::string value_;
	std::string default_;
	friend class Config;
};

class Vector3Property : public Property {
public:
	Vector3Property(const Vector3& value, const std::string& description) : value_(value), default_(value), Property(description) {}
	Vector3 Value() const { return value_; }
	Vector3 DefaultValue() const { return default_; }
private:
	Vector3 value_;
	Vector3 default_;
	friend class Config;
};

class Config {
public:
	BoolProperty* RegisterBool(const std::string& name, const std::string& description, bool defaultValue) {
		BoolProperty* newProp = new BoolProperty(defaultValue, description);
		newProp->guid = guid++;
		properties_[name] = newProp;
		return newProp;
	}

	IntProperty* RegisterInt(const std::string& name, const std::string& description, int defaultValue) {
		IntProperty* newProp = new IntProperty(defaultValue, description);
		newProp->guid = guid++;
		properties_[name] = newProp;
		return newProp;
	}

	FloatProperty* RegisterFloat(const std::string& name, const std::string& description, float defaultValue) {
		FloatProperty* newProp = new FloatProperty(defaultValue, description);
		newProp->guid = guid++;
		properties_[name] = newProp;
		return newProp;
	}

	StringProperty* RegisterString(const std::string& name, const std::string& description, const std::string& defaultValue) {
		StringProperty* newProp = new StringProperty(defaultValue, description);
		newProp->guid = guid++;
		properties_[name] = newProp;
		return newProp;
	}

	Vector3Property* RegisterVector3(const std::string& name, const std::string& description, const Vector3 defaultValue) {
		Vector3Property* newProp = new Vector3Property(defaultValue, description);
		newProp->guid = guid++;
		properties_[name] = newProp;
		return newProp;
	}

	bool sortProperties(std::pair<std::string, Property*> a, std::pair<std::string, Property*> b)
	{
		return a.second->guid < b.second->guid;
	}

	bool SaveToFile(const std::string& filename) const {
		std::ofstream file(filename);
		std::vector<std::pair<std::string, Property*>> sortedProperties(properties_.begin(), properties_.end());
		std::sort(sortedProperties.begin(), sortedProperties.end(), [](std::pair<std::string, Property*> a, std::pair<std::string, Property*> b) { return a.second->guid < b.second->guid; });
		for (const auto& pair : sortedProperties) {
			const std::string& name = pair.first;
			Property* prop = pair.second;
			if (BoolProperty* boolProp = dynamic_cast<BoolProperty*>(prop)) {
				file << "//[Bool] " << boolProp->GetDesc() << " (Default Value: " << (boolProp->DefaultValue() ? "true" : "false") << ")\n";
				file << name << " = " << (boolProp->Value() ? "true" : "false") << "\n\n";
			}
			else if (IntProperty* intProp = dynamic_cast<IntProperty*>(prop)) {
				file << "//[Int] " << intProp->GetDesc() << " (Default Value: " << intProp->DefaultValue() << ")\n";
				file << name << " = " << intProp->Value() << "\n\n";
			}
			else if (FloatProperty* floatProp = dynamic_cast<FloatProperty*>(prop)) {
				file << "//[Float] " << floatProp->GetDesc() << " (Default Value: " << floatProp->DefaultValue() << ")\n";
				file << name << " = " << floatProp->Value() << "\n\n";
			}
			else if (StringProperty* stringProp = dynamic_cast<StringProperty*>(prop)) {
				file << "//[String] " << stringProp->GetDesc() << " (Default Value: \"" << stringProp->DefaultValue() << "\")\n";
				file << name << " = " << stringProp->Value() << "\n\n";
			}
			else if (Vector3Property* vec3Prop = dynamic_cast<Vector3Property*>(prop))
			{
				file << "//[Vector3] " << vec3Prop->GetDesc() << " (Default Value: \"" << vec3Prop->DefaultValue() << "\")\n";
				file << name << " = " << vec3Prop->Value() << "\n\n";
			}
		}

		file.close();

		return file.good();
	}

	bool LoadFromFile(const std::string& filename) {
		std::ifstream file(filename);

		if (!file.good())
		{
			return false;
		}

		std::string line;
		while (std::getline(file, line)) {
			// Ignore comments
			if (line.empty() || line[0] == '/') continue;

			// Split the line into name and value
			std::string::size_type pos = line.find('=');
			if (pos == std::string::npos) continue;  // Invalid line

			std::string name = line.substr(0, pos);
			std::string value = line.substr(pos + 1);

			// Trim whitespace
			name.erase(name.begin(), std::find_if(name.begin(), name.end(), [](char ch){
				return !std::isspace(ch);
				}));
			name.erase(std::find_if(name.rbegin(), name.rend(), [](char ch){
				return !std::isspace(ch);
				}).base(), name.end());

			value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](char ch){
				return !std::isspace(ch);
				}));
			value.erase(std::find_if(value.rbegin(), value.rend(), [](char ch){
				return !std::isspace(ch);
				}).base(), value.end());

			if (properties_.find(name) == properties_.end())
			{
				Logger::log << "[Config] Found invalid property " << name << ", ignoring" << std::endl;
				continue;
			}

			// Update the corresponding property
			Property* prop = properties_[name];
			if (dynamic_cast<BoolProperty*>(prop)) {
				std::transform(value.begin(), value.end(), value.begin(),
					[](unsigned char c) { return std::tolower(c); });
				BoolProperty* bProp = static_cast<BoolProperty*>(prop);
				bProp->value_ = value == "true";
				Logger::log << "[Config] " << name << " = " << (bProp->Value() ? "true" : "false") << ((bProp->Value() != bProp->DefaultValue()) ? "*" : "") << std::endl;
			}
			else if (dynamic_cast<IntProperty*>(prop)) {
				IntProperty* iProp = static_cast<IntProperty*>(prop);
				iProp->value_ = std::stoi(value);
				Logger::log << "[Config] " << name << " = " << iProp->Value() << ((iProp->Value() != iProp->DefaultValue()) ? "*" : "") << std::endl;
			}
			else if (dynamic_cast<FloatProperty*>(prop)) {
				FloatProperty* fProp = static_cast<FloatProperty*>(prop);
				fProp->value_ = std::stof(value);
				Logger::log << "[Config] " << name << " = " << fProp->Value() << ((std::abs(fProp->Value() - fProp->DefaultValue()) > 1e-8) ? "*" : "") << std::endl;
			}
			else if (dynamic_cast<StringProperty*>(prop)) {
				StringProperty* sProp = static_cast<StringProperty*>(prop);
				sProp->value_ = value;
				Logger::log << "[Config] " << name << " = " << sProp->Value() << ((sProp->Value() != sProp->DefaultValue()) ? "*" : "") << std::endl;
			}
			else if (dynamic_cast<Vector3Property*>(prop)) {
				Vector3Property* vProp = static_cast<Vector3Property*>(prop);
				// Find X
				value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](char ch) {
					return !std::isspace(ch) && ch != '(';
				}));

				std::size_t len;
				float x = std::stof(value, &len);

				value.erase(value.begin(), value.begin() + len);

				// Find Y
				value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](char ch) {
					return !std::isspace(ch) && ch != ',';
				}));

				float y = std::stof(value, &len);

				value.erase(value.begin(), value.begin() + len);

				// Find Z
				value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](char ch) {
					return !std::isspace(ch) && ch != ',';
				}));

				float z = std::stof(value);

				vProp->value_ = Vector3(x, y, z);
				Logger::log << "[Config] " << name << " = " << vProp->Value() << ((vProp->Value() != vProp->DefaultValue()) ? "*" : "") << std::endl;
			}
		}

		return true;
	}

	BoolProperty* GetBool(const std::string& name) const {
		return dynamic_cast<BoolProperty*>(properties_.at(name));
	}

	IntProperty* GetInt(const std::string& name) const {
		return dynamic_cast<IntProperty*>(properties_.at(name));
	}

	FloatProperty* GetFloat(const std::string& name) const {
		return dynamic_cast<FloatProperty*>(properties_.at(name));
	}

	StringProperty* GetString(const std::string& name) const {
		return dynamic_cast<StringProperty*>(properties_.at(name));
	}

private:
	std::unordered_map<std::string, Property*> properties_;

	int guid = 0;
};