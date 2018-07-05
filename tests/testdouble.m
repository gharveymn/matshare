classdef testdouble < double
	%TESTDOUBLE Summary of this class goes here
	%   Detailed explanation goes here
	
	properties (SetAccess = immutable)
		enc
	end
	
	methods
		function obj = testdouble(dbl)
			enc = {dbl};
		end
		
		function B = subsref(A,S)
		
		function outputArg = method1(obj,inputArg)
			%METHOD1 Summary of this method goes here
			%   Detailed explanation goes here
			outputArg = obj.Property1 + inputArg;
		end
	end
end

