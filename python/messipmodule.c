#include <stdio.h>

#include "../lib/Src/messip.h"

#include <Python.h>

static messip_cnx_t *cnx = NULL;
static PyObject *builtins = NULL;
static PyObject *eval = NULL;

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

    builtins = PyImport_ImportModule("builtins");
    eval = PyObject_GetAttrString(builtins, "eval");

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
    PyObject *pobj;

    if (!PyArg_ParseTuple(args, "iO", &chn, &pobj)) {
        return NULL;
    }
    printf("chn=%d\n", chn);
    ch = (messip_channel_t *)chn;
    printf("pobj=%p\n", pobj);
    Py_ssize_t len = PyObject_Length(pobj);
    printf("len=%d\n", (int)len);
    Py_ssize_t size = PyObject_Size(pobj);
    printf("size=%d\n", (int)size);

    PyObject* repr = PyObject_Repr(pobj);
    PyObject* str = PyUnicode_AsEncodedString(repr, "utf-8", "~E~");
    const char *bytes = PyBytes_AS_STRING(str);
    #printf("REPR: %s\n", bytes);
    
    // Send message over the channel
    int s = messip_send(ch, 123, bytes, strlen(bytes)+1, &answer, reply_buffer, sizeof(reply_buffer), MESSIP_NOTIMEOUT);

    Py_XDECREF(repr);
    Py_XDECREF(str);

    return Py_BuildValue("i", s);
}

static PyObject* m_receive(PyObject *self, PyObject *args) {

    const int chn;
    messip_channel_t *ch;
    char recv_buffer[200];
    int type;
    PyObject *pobj;

    if (!PyArg_ParseTuple(args, "i", &chn)) {
        return NULL;
    }

    printf("chn=%d\n", chn);
    ch = (messip_channel_t *)chn;

    // Receive message over the channel
    int s = messip_receive(ch, &type, recv_buffer, sizeof(recv_buffer), MESSIP_NOTIMEOUT);
    printf("s = %d\n", s);

    PyObject *rargs = Py_BuildValue("(s)", recv_buffer);
    PyObject *result = PyObject_Call(eval, rargs, NULL);
    return result;
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
    {   
        "receive", m_receive, METH_VARARGS,
        "Receive a message over a channel."
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
