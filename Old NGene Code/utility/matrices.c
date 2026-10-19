/**
 * @file matrices.h
 *
 * @author Mario Nicola
 *
 * @date 23/10/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to manage matrices.
 */

void MatrixInit(double *m,int rows,int cols)
{
	int index;
	int n = rows*cols;

	for(index=0;index<n;index++)
	{
		m[index]=0.0;
	}
}


void MatrixProduct(double *input1,int rows1,int columns1,double *input2,int rows2,int columns2,double *output)
{
	int i, j, k;

	MatrixInit(output,rows1,columns2);
	for(i=0;i<rows1;i++)
	{
		for(j=0;j<columns2;j++)
		{
			for(k=0;k<rows2;k++)
			{
				output[(i*columns2) + j] += (input1[(i*columns1) + k] * input2[(k*columns2) + j]);
			}
		}
	}
}

void MatrixTrasp(double *input, int rows, int columns, double *output)
{
	int i, j;

	for(i = 0; i < rows; i++)
	{
		for(j = 0; j < columns; j++)
		{
			output[j * rows + i] = input[i * columns + j];
		}
	}
}

// Matrix functions implementations
void Interchange(double *c, int size, int row1, int row2)
{
	float temp;
	int i;

	for(i=0;i<size;i++)
	{
		temp = c[row1 * size + i];
		c[row1 * size + i] = c[row2 * size + i];
		c[row2 * size + i] = temp;
	}
}

void MultiplyAdd(double *c, int size, double mult, int row1, int row2)
{
	int i;
	for(i=0;i<size;i++)
	{
		c[row2 * size + i] += mult * c[row1 * size + i];
	}
}

double MatrixDeterminant(double *c, int size)
{
	float d=1;                 /* d is the determinant*/
	int i,j,sign = 1,change;   /* i is just a counter, sign takes care of sign
							    * changes, change takes care of row swapping
							    */
	if(size>2)
	{
		/* if the matrix is 3 x 3 or bigger */
		for(i=0;i<size-1;i++)
		{
			/* go through the first n-1 columns */
			change = 0;            /* reset change */
			if(c[i*size + i])
			{
				/* if c[i,i] is non-zero */
				for(j=i+1;j<size;j++)
				{
					/* do each row */
					MultiplyAdd(c,size,-c[j*size + i]/c[i*size + i],i,j);
					/*make c[i,j] = 0*/
				}
			}
			else
			{
				/* c[i,] = 0 */
				for(j=i+1;j<size;j++)
				{
					/* go down the rows */
					if(c[j*size + i])
					{
						/* looking for a non-zero entry */
						Interchange(c,size,i,j); /* if found switch rows */
						sign *= -1; /* changes sign of det */
						change = 1; /* let program know we found one */
						i--;   /* start over again with c[i,i] != 0 */
						break; /* we're done */
					}
				}
				if(! change)
				{
					/* if we couldn't find a non-zero entry */
					return 0;     /* det is 0*/
				}
			}
		}
		for(i=0;i<size;i++) d *= c[i*size + i]; /* take prod of all c[i,i] */
		return d * sign; /* make sure to mupltiply by sign changes */
	}
	else
	{
		/* if it's 2x2 then the formula is this */
		return c[0]*c[3] - c[2]*c[1];
	}
}

void LDU(double *input, int size, double *l, double *d, double *u)
{
	int i, j, k;
	double partial;

	for(i=0;i<size;i++)
	{
		for(j=i;j<size;j++)
		{
			if(j==i)
			{
				l[i * size + i] = 1;
			}
			else
			{
				l[i * size + j] = 0;
			}
		}

		for(j=0;j<i;j++)
		{
			u[i * size + j] = 0;
		}
		for(j=0;j<size;j++)
		{
			d[i * size + j] = 0;
		}
	}

	for(i=0;i<size;i++)
	{
		for(j=i;j<size;j++)
		{
			partial = 0;

			if(i)
			{
				for(k=0;k<i;k++)
				{
					partial += l[i * size + k] * u[k * size + j];
				}
			}
			u[i * size + j] = input[i * size + j] - partial;
		}

		if(i!=size - 1)
		{

			for(j=i + 1;j<size;j++)
			{
				partial = 0;

				if(i)
				{
					for(k=0;k<i;k++)
					{
						partial += l[j * size + k] * u[k * size + i];
					}
				}
				l[j * size + i] = (input[j * size + i] - partial) / u[i * size + i];
			}
		}
	}

	for(i=0;i<size;i++)
	{
		d[i * size + i] = u[i * size + i];

		partial = u[i * size + i];

		if(partial)
		{
			for(j=i;j<size;j++)
			{
				u[i * size + j] /= partial;
			}
		}
		else
		{
			u[i * size + i] = 1;
		}
	}
}

void invL(double *input, int size, double *output)
{
	int i, j, k;
	double partial;

	for(i=0;i<size;i++)
	{
		for(j=0;j<size;j++)
		{
			output[i * size + j] = 0;
		}
	}
	for(i=0;i<size;i++)
	{
		for(j=0;j<=i;j++)
		{
			if(j < i)
			{
				partial = 0;
				if(i - j > 1)
				{
					for(k=j + 1;k < i;k++)
					{
						partial += input[i * size + k] * output[k * size + j];
					}
				}
				output[i * size + j] = - input[i * size + j] - partial;
			}
			else
			{
				output[i * size +i] = 1;
			}
		}
	}
}

void invD(double *input, int size, double *output)
{
	int i, j;

	for(i=0;i<size;i++)
	{
		for(j=0;j<size;j++)
		{
			output[i * size + j] = 0;
		}
		output[i * size + i] = 1 / input[i * size + i];
	}
}

void MatrixInv(double *input, int size, double *output, double *l, double *d, double *u, double *u_t, double *il, double *id, double *iu, double *iu_t, double *iud, double *det)
{
	int i, j;

	for(i=0;i<size;i++)
	{
		for(j=0;j<size;j++)
		{
			det[i * size + j] = input[i * size + j];
		}
	}

	if(MatrixDeterminant(det,size))
	{
		// LDU factorization
		LDU(input, size, l, d, u);
		// U transpose
		MatrixTrasp(u, size, size, u_t);
		// U^-1 transpose
		invL(u_t, size, iu_t);
		// U^-1
		MatrixTrasp(iu_t, size, size, iu);
		// D^-1
		invD(d, size, id);
		// L^-1
		invL(l, size, il);
		// U^-1 * D^-1
		MatrixProduct(iu, size, size, id, size, size, iud);
		// (U^-1 * D^-1) * L^-1
		MatrixProduct(iud, size, size, il, size, size, output);
	}
}

void MatrixSum(double *input1, int rows1, int columns1, double *input2, int rows2, int columns2, double *output)
{
	int i, j;

	if((rows1!=rows2) || (columns1!=columns2))
	{
		return;
	}

	for(i=0;i<rows1;i++)
	{
		for(j=0;j<columns1;j++)
		{
			output[(i*columns1)+j] = input1[(i*columns1)+j] + input2[(i*columns1)+j];
		}
	}
}

void MatrixDiff(double *input1, int rows1, int columns1, double *input2, int rows2, int columns2, double *output)
{
	int i, j;

	if((rows1!=rows2) || (columns1!=columns2))
	{
		return;
	}
	else
	{
		for(i=0;i<rows1;i++)
		{
			for(j=0;j<columns1;j++)
			{
				output[(i*columns1)+j] = input1[(i*columns1)+j] - input2[(i*columns1)+j];
			}
		}
	}
}

void MergeMatricesVertically(double *input1, int rows1, int columns1, double *input2, int rows2, int columns2, double *output)
{
	int i, j;

	if(columns1!=columns2)
	{
		return;
	}
	else
	{
		for(i=0;i<rows1*columns1;i++)
		{
			output[i] = input1[i];
		}
		for(j=0;j<rows2*columns2;j++)
		{
			output[j+i] = input2[j];
		}
	}
}

void MatrixIdentity(double *output, int size)
{
	int i, j;

	for(i=0;i<size;i++)
	{
		for(j=0;j<size;j++)
		{
			if(i==j)
			{
				output[(i*size)+i] = 1.0;
			}
			else
			{
				output[(i*size)+j] = 0.0;
			}
		}
	}
}
