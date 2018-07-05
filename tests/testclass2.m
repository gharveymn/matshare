classdef testclass2
	%TESTCLASS2 Summary of this class goes here
	%   Detailed explanation goes here
	
	properties
		Property1
	end
	
	methods
		function obj = testclass2(inputArg1,inputArg2)
			%TESTCLASS2 Construct an instance of this class
			%   Detailed explanation goes here
			obj.Property1 = inputArg1 + inputArg2;
		end
		
		function outputArg = method1(obj,inputArg)
			%METHOD1 Summary of this method goes here
			%   Detailed explanation goes here
			outputArg = obj.Property1 + inputArg;
		end
	end
	
	methods (Static)
		function [outputArg1,outputArg2] = func(inputArg1,inputArg2)
			%FUNC Summary of this function goes here
			%   Detailed explanation goes here
			outputArg1 = inputArg1;
			outputArg2 = inputArg2;
		end
	end
end

