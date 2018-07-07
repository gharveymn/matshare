function [ret] = variablegenerator(rns, maxDepth, maxElems, maxDims, maxChildren, ignoreUnusables, typespec)
	[ret] = variablegenerator_(1, rns, maxDepth, maxElems, maxDims, maxChildren, ignoreUnusables, typespec);
end


function [ret] = variablegenerator_(currDepth, rns, maxDepth, maxElems, maxDims, maxChildren, ignoreUnusables, typespec)

	%  Variable Type Key
	%    1    mxCELL_CLASS,
	%    2    mxSTRUCT_CLASS,
	% 	3	mxLOGICAL_CLASS,
	% 	4	mxCHAR_CLASS,
	% 	5	mxVOID_CLASS,
	% 	6	mxDOUBLE_CLASS,
	% 	7	mxSINGLE_CLASS,
	% 	8	mxINT8_CLASS,
	% 	9	mxUINT8_CLASS,
	% 	10	mxINT16_CLASS,
	% 	11	mxUINT16_CLASS,
	% 	12	mxINT32_CLASS,
	% 	13	mxUINT32_CLASS,
	% 	14	mxINT64_CLASS,
	% 	15	mxUINT64_CLASS,
	% 	16	mxFUNCTION_CLASS,
	% 	17	mxOPAQUE_CLASS/SPARSES,
	% 	18	mxOBJECT_CLASS,

	if(maxDepth <= currDepth)
		%dont make another layer
		if(typespec > 2)
			randvartype = typespec;
		else
			if(ignoreUnusables)
				randvartype = randi(rns, 13) + 2;
			else
				randvartype = randi(rns, 16) + 2;
			end
		end
	else
		if(typespec > 0)
			randvartype = typespec;
		else
			if(ignoreUnusables)
				randvartype = randi(rns, 15);
			else
				randvartype = randi(rns, 18);
			end
		end
	end

	if(randvartype > 2)
		thisMaxElements = maxElems;
	else
		%max elems for structs
		thisMaxElements = maxChildren;
	end
	
	numdims = randi(rns, [2,maxDims]);
	dims = ones(1, numdims);
	
	numvarsz = randi(rns, thisMaxElements + 1) - 1;
	nums = 1:numvarsz;
	divs = nums(mod(numvarsz, nums) == 0);
	temp = numvarsz;
	if(numvarsz ~= 0)
		for i = 1:numdims
			randdiv = divs(randi(rns, numel(divs)));
			dims(i) = randdiv;
			temp = temp / randdiv;
			divs = divs(mod(temp,divs) == 0);
			if(temp == 1)
				break;
			end
		end
		if(temp ~= 1)
			dims(end) = temp;
		end
	else
		dims = randi(rns, intmax,1,numdims) - 1;
		dims(randi(rns, numdims)) = 0;
	end
	
	makesparse = (rand(rns) < 0.5) & (numdims == 2);

	switch(randvartype)
		case(1)
			% 	1	mxCELL_CLASS
			ret = cell(dims);
			for k = 1:numel(ret)
				ret{k} = variablegenerator_(currDepth + 1, rns, maxDepth, maxElems, maxDims, maxChildren, ignoreUnusables, typespec);
			end
		case(2)
			% 	2	mxSTRUCT_CLASS
			numPossibleFields = 5;
			if(numvarsz ~= 0)
				possibleFields = {'cat',[],'dog',[],'fish',[],'cow',[],'twentyonesavage',[]};
				cdims = num2cell(dims);
				ret(cdims{:}) = struct(possibleFields{1:2*randi(rns, numPossibleFields)});
				retFields = fieldnames(ret);
				for k = 1:numel(retFields)
					for j = 1:numel(ret)
						ret(j).(retFields{k}) = variablegenerator_(currDepth + 1, rns, maxDepth, maxElems, maxDims, maxChildren, ignoreUnusables, typespec);
					end

				end
			else
				possibleFields = {'cat',dims,'dog',dims,'fish',dims,'cow',dims,'twentyonesavage',dims};
				ret = struct(possibleFields{1:2*randi(rns, numPossibleFields)});
			end
		case(3)
			% 	3	mxLOGICAL_CLASS,
			if(makesparse)
				if(numvarsz == 0)
					% prevent matlab from allocating too much
					dims = mod(dims, 100000);
				end
				ret = sprand(dims(1),dims(2),rand(rns)) > 0.5;
			else
				ret = rand(dims) > 0.5;
			end
		case(4)
			% 	4	mxCHAR_CLASS,
			ret = char(randi(rns, [0,65535],dims));
		case(5)
			% 	5	mxVOID_CLASS,
			%reserved, make a double instead
			ret = rand(rns, dims,'double');
		case(6)
			% 	6	mxDOUBLE_CLASS,
			if(makesparse)
				if(numvarsz == 0)
					% prevent matlab from allocating too much
					dims = mod(dims, 100000);
				end
				ret = sprand(dims(1),dims(2),rand(rns));
			else
				ret = rand(rns, dims,'double');
			end
		case(7)
			% 	7	mxSINGLE_CLASS,
			ret = rand(rns, dims,'single');
		case(8)
			% 	8	mxINT8_CLASS,
			ret = randi(rns, intmax('int8'),dims,'int8');
		case(9)
			% 	9	mxUINT8_CLASS,
			ret = randi(rns, intmax('uint8'),dims,'uint8');
		case(10)
			% 	10	mxINT16_CLASS,
			ret = randi(rns, intmax('int16'),dims,'int16');
		case(11)
			% 	11	mxUINT16_CLASS,
			ret = randi(rns, intmax('uint16'),dims,'uint16');
		case(12)
			% 	12	mxINT32_CLASS,
			ret = randi(rns, intmax('int32'),dims,'int32');
		case(13)
			% 	13	mxUINT32_CLASS,
			ret = randi(rns, intmax('uint32'),dims,'uint32');
		case(14)
			% 	14	mxINT64_CLASS,
			% bitshift and bitor to convert into a proper int64        
			ret = int64(bitor(bitshift(randuint64shifted(rns, dims),32), randuint64shifted(rns, dims)));
		case(15)
			% 	15	mxUINT64_CLASS,
			% bitshift and bitor to convert into a proper uint64        
			ret = bitor(bitshift(randuint64shifted(rns, dims),32), randuint64shifted(rns, dims));
		case(16)
			% 	16	mxFUNCTION_CLASS,
			af1 = @(x) x + 17;
			af2 = @(x,y,z) x*y*z;
			sf1 = @simplefunction1;
			sf2 = @simplefunction2;

			funcs = {af1,af2,sf1,sf2};
			ret = funcs{randi(rns, numel(funcs))};
		case(17)
			% 	17	mxOPAQUE_CLASS,
			% not sure how to generate, generate an object array instead
			if(numvarsz ~= 0)
				cdims = num2cell(dims);
				ret = BasicClass(randi(rns, intmax),cdims);
			else
				ret = [];
			end
		case(18)
			% 	18	mxOBJECT_CLASS,
			if(numvarsz ~= 0)
				cdims = num2cell(dims);
				ret = BasicClass(randi(rns, intmax),cdims);
			else
				ret = [];
			end
	end

end

