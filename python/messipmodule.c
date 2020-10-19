#include <stdio.h>

#include "../lib/Src/messip.h"

#include <Python.h>

static messip_cnx_t *cnx = NULL;

static PyObject* m_connect(PyObject *self, PyObject *args) {
    const char *mgr_ref;
    if (!PyArg_ParseTuple(args, "s", &mgr_ref)) {
        return NULL;
    }

    printf("mgr_ref=%s\n", mgr_ref);

    // Connect to one messip server
    messip_init();
    cnx = messip_connect( NULL, mgr_ref, MESSIP_NOTIMEOUT );
    if ( !cnx ) {
        printf( "Unable to find messip manager\n" );
    }

    Py_RETURN_NONE;
}

static PyObject* m_channel_create(PyObject *self, PyObject *args) {
    const char *channel_name;
    if (!PyArg_ParseTuple(args, "s", &channel_name)) {
        return NULL;
    }

    printf("channel_name=%s\n", channel_name);

    // Create the channel
    messip_channel_t *ch = messip_channel_create(cnx, channel_name, MESSIP_NOTIMEOUT, 0);
    if ( !ch ) {
        printf( "Unable to create channel\n" );
    }

    return Py_BuildValue("i", ch);
}

static PyObject* m_channel_connect(PyObject *self, PyObject *args) {
    const char *channel_name;
    if (!PyArg_ParseTuple(args, "s", &channel_name)) {
        return NULL;
    }

    printf("channel_name=%s\n", channel_name);

    // Connect to the channel
    messip_channel_t *ch = messip_channel_connect(cnx, channel_name, MESSIP_NOTIMEOUT);
    if ( !ch ) {
        printf( "Unable to connect to the channel\n" );
    }

    return Py_BuildValue("i", ch);
}

static PyObject* m_send(PyObject *self, PyObject *args) {

    const int chn;
    messip_channel_t *ch;
    char send_buffer[] = "Hello";
    int answer;
    char reply_buffer[50];

    if (!PyArg_ParseTuple(args, "i", &chn)) {
        return NULL;
    }

    printf("chn=%d\n", chn);
    ch = (messip_channel_t *)chn;

    // Semd message overthe channel
    int s = messip_send(ch, 123, send_buffer, 5, &answer, reply_buffer, sizeof(reply_buffer), MESSIP_NOTIMEOUT);
    if ( !ch ) {
        printf( "Unable to connect to the channel\n" );
    }

    return Py_BuildValue("i", s);
}

// Method definition object for this extension, these argumens mean:
// ml_name: The name of the method
// ml_meth: Function pointer to the method implementation
// ml_flags: Flags indicating special features of this method, such as
//          accepting arguments, accepting keyword arguments, being a
//          class method, or being a static method of a class.
// ml_doc:  Contents of this method's docstring
static PyMethodDef messip_methods[] = { 
    {   
        "connect", m_connect, METH_VARARGS,
        "Connect to the messip manager."
    },  
    {   
        "channel_create", m_channel_create, METH_VARARGS,
        "Create a channel."
    },  
    {   
        "channel_connect", m_channel_connect, METH_VARARGS,
        "Connect to a channel."
    },  
    {   
        "send", m_send, METH_VARARGS,
        "Send a message over a channel."
    },  
    {NULL, NULL, 0, NULL}
};

// Module definition
// The arguments of this structure tell Python what to call your extension,
// what it's methods are and where to look for it's method definitions
static struct PyModuleDef messip_definition = { 
    PyModuleDef_HEAD_INIT,
    "messip",
    "A Python module which implements Message-Passing.",
    -1, 
    messip_methods
};

// Module initialization
// Python calls this function when importing your extension. It is important
// that this function is named PyInit_[[your_module_name]] exactly, and matches
// the name keyword argument in setup.py's setup() call.
PyMODINIT_FUNC PyInit_messip(void) {
    Py_Initialize();
    return PyModule_Create(&messip_definition);
}
