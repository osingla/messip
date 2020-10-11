#define PY_SSIZE_T_CLEAN
#include <Python.h>

static PyObject *
messip_connect(PyObject *self, PyObject *args)
{
    const char *command;
    int sts;

    if (!PyArg_ParseTuple(args, "s", &command))
        return NULL;
    sts = 0;
    return PyLong_FromLong(sts);
}

static PyMethodDef MessipMethods[] = {

    {"connect",  messip_connect, METH_VARARGS,
     "Execute a shell command."},

    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static struct PyModuleDef messipmodule = {
    PyModuleDef_HEAD_INIT,
    "messip",   /* name of module */
    NULL, /* module documentation, may be NULL */
    -1,       /* size of per-interpreter state of the module,
                 or -1 if the module keeps state in global variables. */
    MessipMethods
};

PyMODINIT_FUNC
PyInit_messip(void)
{
    return PyModule_Create(&messipmodule);
}
