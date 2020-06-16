#pragma once

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

void throw_error(errors);