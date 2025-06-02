/*****************************************************************************
 * pdb - routines for maintaining a database of performance information
 *****************************************************************************/



#ifndef __PDB_H__
#define __PDB_H__
#ifdef __cplusplus
extern "C" {
#endif



typedef void (*pdbCallbackT)();

typedef int pdbStatusT;
#define PDB_NO_ERROR		0x0000
#define PDB_OUT_OF_MEMORY	0x0001	/* malloc failed */
#define PDB_SYNTAX_ERROR	0x0002	/* syntax error in database file */
#define PDB_NOT_FOUND		0x0004	/* no such benchmark in database */
#define PDB_CANT_WRITE		0x0008	/* can't update database file */
#define PDB_NOT_OPEN		0x0010	/* database not yet open */
#define PDB_ALREADY_OPEN	0x0020	/* database already open */



extern pdbStatusT pdbClose	(void);
extern pdbStatusT pdbMeasureRate(pdbCallbackT initialize,
				 pdbCallbackT operation,
				 pdbCallbackT finalize,
				 int calibrate,
				 double* rate);
extern pdbStatusT pdbOpen	(void);
extern pdbStatusT pdbReadRate	(const char* machineName,
				 const char* applicationName,
				 const char* benchmarkName,
				 const char* versionString,
				 double* rate);
extern pdbStatusT pdbWriteRate	(const char* machineName,
				 const char* applicationName,
				 const char* benchmarkName,
				 const char* versionString,
				 const double rate);



#ifdef __cplusplus
}
#endif
#endif /* !__PDB_H__ */


