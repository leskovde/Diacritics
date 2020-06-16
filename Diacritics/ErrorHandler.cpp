#include <cstdlib>
#include <ostream>
#include <iostream>

extern const char* model_name;
extern const char* offset_model_name;
extern const char* dictionary_name;

enum class errors
{
	input_file_error,
	output_file_error,
	model_error,
	offset_model_error,
	dictionary_error,
	invalid_option_error,
	multithreading_error
};

/**
 * Outputs a message based on the error passed as the argument and terminates the program
 */
void throw_error(const errors error)
{
	std::wcerr << L"ERROR:\t";

	switch (error)
	{
	case errors::input_file_error:
		std::wcerr << L"Input file could not be read.";
		break;
	case errors::output_file_error:
		std::wcerr << L"Could not write to the output file.";
		break;
	case errors::model_error:
		std::wcerr << L"Model file could not be read. Make sure that the " << model_name <<
			L" file is present in the directory containing the 'diac' executable file.";
		break;
	case errors::offset_model_error:
		std::wcerr << L"Offset model file could not be read. Make sure that the " << offset_model_name <<
			L" file is present in the directory containing the 'diac' executable file.";
		break;
	case errors::dictionary_error:
		std::wcerr << L"Dictionary file could not be read. Make sure that the " << dictionary_name <<
			L" file is present in the directory containing the 'diac' executable file.";
		break;
	case errors::invalid_option_error:
		std::wcerr << L"Program has detected an invalid user option. Consult 'diac --help' for more information.";
		break;
	case errors::multithreading_error:
		std::wcerr << L"Some words have been lost due to multi threading. This should not have happened.";
		break;

	default:
		break;
	}

	std::wcerr << std::endl;
	exit(1);
}