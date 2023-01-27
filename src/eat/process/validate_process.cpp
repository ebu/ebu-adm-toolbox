#include "eat/process/validate_process.hpp"

#include <iostream>

#include "eat/framework/exceptions.hpp"
#include "eat/process/validate.hpp"

using namespace eat::framework;
using namespace eat::process::profiles;
using namespace eat::process::validation;

namespace eat::process {

class Validate : public FunctionalAtomicProcess {
 public:
  Validate(const std::string &name, const Profile &profile)
      : FunctionalAtomicProcess(name),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")),
        validator(make_profile_validator(profile)) {}

  void process() override {
    auto adm = std::move(in_axml->get_value());

    auto result = validator.run(adm);

    if (any_messages(result)) {
      format_results(std::cerr, result, false);
      throw ValidationError{"found errors in document; see above"};
    }
  }

 private:
  DataPortPtr<ADMData> in_axml;
  ProfileValidator validator;
};

framework::ProcessPtr make_validate(const std::string &name, const Profile &profile) {
  return std::make_shared<Validate>(name, profile);
}
}  // namespace eat::process
